/*
Copyright (c) 2014, LiteSpeed Technologies Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met: 

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer. 
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution. 
    * Neither the name of the Lite Speed Technologies Inc nor the
      names of its contributors may be used to endorse or promote
      products derived from this software without specific prior
      written permission.  

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
#include "../include/ls.h"
#include <string.h>
#define     MNAME       hellohandler2
struct lsi_module_t MNAME;

int reg_handler(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    if ( strstr(uri, ".345") != 0 )
    {
        g_api->register_req_handler(rec->_session, &MNAME, 0);
        g_api->session_log(rec->_session, LSI_LOG_DEBUG, "[hellohandler2:%s] register_req_handler fot URI: %s\n", 
                   MNAME._info, g_api->get_req_uri(rec->_session));
    }
    return LSI_RET_OK;
}

static int _init()
{
    g_api->add_hook( LSI_HKPT_RECV_REQ_HEADER, &MNAME, reg_handler, LSI_HOOK_NORMAL, 0 );
    g_api->log(LSI_LOG_DEBUG, "[hellohandler2:%s] _init [log in module code]", MNAME._info);
    return 0;
}

int handlerBeginProcess(void *session)
{
    g_api->append_resp_body( session, "Hello module handler2.\r\n", 24 ); 
    g_api->end_resp(session);
     g_api->session_log(session, LSI_LOG_DEBUG, "[hellohandler2:%s] handlerBeginProcess fot URI: %s\n", 
                   MNAME._info, g_api->get_req_uri(session));
    return 0;
}
/**
 * Define a handler, need to provide a struct lsi_handler_t object, in which 
 * the first function pointer should not be NULL
 */
lsi_handler_t myhandler = { handlerBeginProcess, NULL, NULL };
lsi_module_t MNAME = { LSI_MODULE_SIGNATURE, _init, &myhandler, NULL, "version 1.0" };
