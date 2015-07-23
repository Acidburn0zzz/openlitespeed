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
#define _GNU_SOURCE
#include "../include/ls.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define MNAME       uploadchecker
lsi_module_t MNAME;

static int is_bad_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    /**
     * In this example, we check if this file contains a string "_BAD_FILE_"
     * if yes, treat it as a bad file
     */
    int ret = 0;
    char buf[8192];
    int len;
    while (len = fread(buf, 1, 8191, fp), len > 0)
    {
        buf[len] = 0x00;
        if (strstr(buf, "_BAD_FILE_"))
        {
            ret = 1;
            break;
        }
    }

    fclose(fp);
    return ret;
}


int check_req_uploaded_file(lsi_cb_param_t *param)
{
    char *name, *val, *path;
    int nameLen, valLen;
    int i;
    int count = g_api->get_req_body_part_count(param->_session);
    for (i = 0; i < count; ++i)
    {
        if (g_api->is_req_body_part_file(param->_session, i))
        {
            g_api->get_req_body_part(param->_session, i, &name, &nameLen,
                                     &val, &valLen, &path);

            if (is_bad_file(path))
                return LSI_HK_RET_ERROR;
        }
    }
    return LSI_HK_RET_OK;
}

static int set_session(lsi_cb_param_t *param)
{
    g_api->set_parse_req_body(param->_session);
    return LSI_HK_RET_OK;
}

static int _init(lsi_module_t *pModule)
{
    return 0;
}


static lsi_serverhook_t server_hooks[] =
{
    { LSI_HKPT_HTTP_BEGIN, set_session, LSI_HOOK_NORMAL, LSI_HOOK_FLAG_ENABLED },
    { LSI_HKPT_RCVD_REQ_BODY, check_req_uploaded_file, LSI_HOOK_EARLY, LSI_HOOK_FLAG_ENABLED },
    lsi_serverhook_t_END   //Must put this at the end position
};

lsi_module_t MNAME =
{
    LSI_MODULE_SIGNATURE, _init, NULL, NULL, "", server_hooks
};

