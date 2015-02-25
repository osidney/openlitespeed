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
#include "../include/ls.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/**
 * Define the module name, MUST BE the same as .so file name;
 * i.e., if MNAME is testmodule, then the .so must be testmodule.so.
 *
 * This example is for testing reply a big buffer (20MB)
 * Since the buffer is so big, we can not append all the reply data
 * to the response cache.
 * So on_write event needs to be used to write the remaining data
 */

#define MNAME       replybigbufhandler
lsi_module_t MNAME;

static char testuri[] = "/replybigbuf";

#define REPLY_BUFFER_LENGTH (1*1024*1024)


static int free_mydata(void *data)
{
    // The data is only the size, did not malloc memory, so no free needed
    return 0;
}


static int disable_compress(lsi_cb_param_t *param)
{
    // To disable compress, set the LSI_HOOK_FLAG_DECOMPRESS_REQUIRED flag
    //   in the hook.
    return LSI_HK_RET_OK;
}


static int reg_handler(lsi_cb_param_t *param)
{
    const char *uri;
    int len;
    uri = g_api->get_req_uri(param->_session, &len);
    if ((len >= sizeof(testuri) - 1)
        && (strncasecmp(uri, testuri, sizeof(testuri) - 1) == 0))
    {
        g_api->register_req_handler(param->_session, &MNAME, sizeof(testuri) - 1);
        g_api->set_module_data(param->_session, &MNAME, LSI_MODULE_DATA_HTTP,
                               (void *)REPLY_BUFFER_LENGTH);
    }
    return LSI_HK_RET_OK;
}


static int _init(lsi_module_t *module)
{
    g_api->init_module_data(module, free_mydata, LSI_MODULE_DATA_HTTP);
    return 0;
}


// begin_process will be called the first time,
//   then on_write will be called next and next.
static char txt1[] = "replybigbufhandler module reply the first line\r\n";
static int begin_process(lsi_session_t *session)
{
    g_api->set_resp_header(session, LSI_RESP_HEADER_CONTENT_TYPE, NULL, 0,
                           "text/html", 9, LSI_HEADER_SET);
    int size = sizeof(txt1) - 1;
    long left = (long)g_api->get_module_data(session, &MNAME,
                LSI_MODULE_DATA_HTTP);
    g_api->append_resp_body(session, txt1, size);
    left -= size;
    g_api->set_module_data(session, &MNAME, LSI_MODULE_DATA_HTTP,
                           (void *)left);
    g_api->flush(session);
    return 0;
}


// return 0: error, 1: done, 2: not finished, definitions are in ls.h
static int on_write(lsi_session_t *session)
{
#define _BLOCK_SIZE    (1024)
    int i;
    char buf[_BLOCK_SIZE] = {0};
    int size = 0;
    long left = (long)g_api->get_module_data(session, &MNAME,
                LSI_MODULE_DATA_HTTP);

    while (left > 0 && g_api->is_resp_buffer_available(session))
    {
        for (i = 0; i < _BLOCK_SIZE; ++i)
            buf[i] = 'A' + rand() % 52;
        size = ((left > _BLOCK_SIZE) ? _BLOCK_SIZE : left);
        g_api->append_resp_body(session, buf, size);
        left -= size;
    }
    g_api->set_module_data(session, &MNAME, LSI_MODULE_DATA_HTTP,
                           (void *)left);
    if (left == 0)
        g_api->append_resp_body(session, "\r\nEnd\r\n", 7);

    return (left > 0) ?  LSI_WRITE_RESP_CONTINUE : LSI_WRITE_RESP_FINISHED;
}


static lsi_serverhook_t server_hooks[] =
{
    { LSI_HKPT_RECV_REQ_HEADER, reg_handler, LSI_HOOK_NORMAL, LSI_HOOK_FLAG_ENABLED },
    { LSI_HKPT_RCVD_RESP_BODY, disable_compress, LSI_HOOK_NORMAL, LSI_HOOK_FLAG_DECOMPRESS_REQUIRED | LSI_HOOK_FLAG_ENABLED },
    lsi_serverhook_t_END   //Must put this at the end position
};

lsi_handler_t myhandler = { begin_process, NULL, on_write, NULL };
lsi_module_t MNAME =
{
    LSI_MODULE_SIGNATURE, _init, &myhandler, NULL, "", server_hooks
};

