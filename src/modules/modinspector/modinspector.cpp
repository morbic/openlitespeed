/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2015  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/

#include <ls.h>
#include <lsr/ls_str.h>
#include <lsr/ls_strtool.h>
#include <lsr/ls_confparser.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define MNAME       modinspector
extern lsi_module_t MNAME;
#define DEF_SCANNER_PATH        "/usr/bin/clamdscan"
#define DEF_SCANNER_PREFIX      "Infected files:"


struct scanner_param_st
{
    char *path;
    char *prefix;
};


const int paramArrayCount = 2;
const char *paramArray[paramArrayCount + 1] =
{
    "scannerpath",
    "counterprefix",
    NULL //Must have NULL in the last item
};


static int modinspector_parseList(ls_objarray_t *pList,
                                  scanner_param_st *pConfig)
{
    int count = ls_objarray_getsize(pList);
    if (count != 2)
        return -1;

    ls_str_t *p = (ls_str_t *)ls_objarray_getobj(pList, 0);
    for (int i = 0 ; i < paramArrayCount; ++i)
    {
        if (ls_str_len(p) == (int)strlen(paramArray[i]) &&
            strncasecmp(paramArray[i], ls_str_cstr(p), ls_str_len(p)) == 0)
        {
            p = (ls_str_t *)ls_objarray_getobj(pList, 1);
            if (i == 0)
                pConfig->path = strndup(ls_str_cstr(p), ls_str_len(p));
            else //i == 1
                pConfig->prefix = strndup(ls_str_cstr(p), ls_str_len(p));
            break;
        }
    }
    return 0;
}


static void *modinspector_parseConfig(const char *param, int param_len,
                                      void *_initial_config, int level, const char *name)
{
    ls_confparser_t confparser;
    scanner_param_st *initConf = (scanner_param_st *)_initial_config;
    scanner_param_st *myConf = (scanner_param_st *) malloc(sizeof(
                                   scanner_param_st));
    if (!myConf)
        return NULL;

    if (param)
    {
        ls_confparser(&confparser);

        const char *pBufBegin = param, *pBufEnd = param + param_len;
        const char *pLine, *pLineEnd;
        while ((pLine = ls_getconfline(&pBufBegin, pBufEnd, &pLineEnd)) != NULL)
        {
            ls_objarray_t *pList = ls_confparser_line(&confparser, pLine, pLineEnd);
            if (!pList)
                continue;

            if (0 != modinspector_parseList(pList, myConf))
            {
                g_api->log(NULL, LSI_LOG_ERROR,
                           "[modinspector]modinspector_parseConfig find error in line: \"%.*s\"\n",
                           pLineEnd - pLine, pLine);
            }
        }
        ls_confparser_d(&confparser);
    }

    if (!myConf->path)
        myConf->path = strdup(initConf ? initConf->path : DEF_SCANNER_PATH);
    if (!myConf->prefix)
        myConf->prefix = strdup(initConf ? initConf->prefix : DEF_SCANNER_PREFIX);

    return (void *)myConf;
}

static void modinspector_freeConfig(void *_config)
{
    scanner_param_st *pConfig = (scanner_param_st *)_config;
    if (pConfig)
    {
        if (pConfig->path)
            free(pConfig->path);
        if (pConfig->prefix)
            free(pConfig->prefix);
        free(pConfig);
    }
}


static int my_cb(lsi_session_t *session, const long lParam, void *pParam)
{
    int len;
    int count = 0;
    const char *prefix = (const char *)lParam;
    char *buf = g_api->get_ext_cmd_res_buf(session, &len);
    g_api->log(session, LSI_LOG_DEBUG,
               "[modinspector]result:\n %.*s\n",
               (len > 1024 ? 1024 : len),  buf);
    char *p = strstr(buf, prefix);
    if (!p)
    {
        g_api->log(session, LSI_LOG_ERROR,
                   "[modinspector]result does NOT contains prefix \"%s\".\n",
                   prefix);
    }
    else
    {
        sscanf(p + strlen(prefix), "%d", &count);
        if (count)
        {
            g_api->log(session, LSI_LOG_WARN,
                       "[modinspector]##--parsed Infections file number: [%d]--##\n", count);
            count = -1;
        }
    }

    g_api->resume(session, count);
    return 0;
}


static int checkFiles(lsi_param_t *param, const char *cmd, int len,
                      const char *prefix)
{
    g_api->log(param->session, LSI_LOG_DEBUG,
               "[modinspector]checkFiles: %.*s\n",
               len, cmd);
    return g_api->exec_ext_cmd(param->session, cmd, len, my_cb, (long)prefix,
                               NULL);
}


static int check_req_uploaded_file(lsi_param_t *param)
{
    char *name, *val, *path;
    int nameLen, valLen;
    int i, file_count = 0;

    scanner_param_st *scanner_st = (scanner_param_st *)
                                   g_api->get_config(param->session,
                                           &MNAME);

    ls_str_t *str = ls_str_new(scanner_st->path, strlen(scanner_st->path));
    int count = g_api->get_req_body_part_count(param->session);

    for (i = 0; i < count; ++i)
    {
        if (g_api->is_req_body_part_file(param->session, i))
        {
            g_api->get_req_body_part(param->session, i, &name, &nameLen,
                                     &val, &valLen, &path);
            ls_str_append(str, " ", 1);
            ls_str_append(str, path, strlen(path));
            ++file_count;
        }
    }

    if (file_count > 0)
    {
        int ret = checkFiles(param, ls_str_cstr(str), ls_str_len(str),
                             scanner_st->prefix);
        ls_str_delete(str);
        if (ret == 0)
            return LSI_SUSPEND;
        else
            return LSI_OK;
    }
    else
    {
        ls_str_delete(str);
        return LSI_OK;
    }
}

static int set_session(lsi_param_t *param)
{
    /**
     * If config is not correct, quit!!!
     */
    scanner_param_st *scanner_st = (scanner_param_st *)
                                   g_api->get_config(param->session,
                                           &MNAME);
    if (scanner_st && scanner_st->path && scanner_st->prefix)
    {
        g_api->set_parse_req_body(param->session);

        int aEnableHkpts[1];
        aEnableHkpts[0] = LSI_HKPT_RCVD_REQ_BODY;
        g_api->enable_hook(param->session, &MNAME, 1,
                           aEnableHkpts, 1);
    }
    return LSI_OK;
}

static int _init(lsi_module_t *pModule)
{
    return 0;
}


static lsi_serverhook_t server_hooks[] =
{
    { LSI_HKPT_HTTP_BEGIN, set_session, LSI_HOOK_NORMAL, LSI_FLAG_ENABLED },
    { LSI_HKPT_RCVD_REQ_BODY, check_req_uploaded_file, LSI_HOOK_EARLY, 0 },
    LSI_HOOK_END   //Must put this at the end position
};

lsi_confparser_t testparam_dealConfig = { modinspector_parseConfig, modinspector_freeConfig, paramArray };
lsi_module_t MNAME =
{
    LSI_MODULE_SIGNATURE, _init, NULL, &testparam_dealConfig, "v1.0", server_hooks, { 0 }
};


