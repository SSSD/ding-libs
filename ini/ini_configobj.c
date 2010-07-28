/*
    INI LIBRARY

    Module represents interface to the main INI object.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2010

    INI Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    INI Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with INI Library.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "config.h"
#include "trace.h"
#include "collection.h"
#include "ini_config_priv.h"
#include "ini_defines.h"
#include "ini_valueobj.h"

/* This constant belongs to ini_defines.h. Move from ini_config - TBD */
#define COL_CLASS_INI_BASE        20000
#define COL_CLASS_INI_CONFIG      COL_CLASS_INI_BASE + 0

/* Callback */
void ini_cleanup_cb(const char *property,
                    int property_len,
                    int type,
                    void *data,
                    int length,
                    void *custom_data)
{
    struct value_obj *vo = NULL;

    TRACE_FLOW_ENTRY();

    /* Banary items are the values */
    if(type == COL_TYPE_BINARY) {
        vo = *((struct value_obj **)(data));
        value_destroy(vo);
    }

    TRACE_FLOW_EXIT();
}

/* Traverse the collection and clean the object */
void ini_config_destroy(struct configobj *ini_config)
{
    TRACE_FLOW_ENTRY();

    if (ini_config) {
        if(ini_config->cfg) {

            col_destroy_collection_with_cb(ini_config->cfg,
                                           ini_cleanup_cb,
                                           NULL);
        }
        free(ini_config);
    }

    TRACE_FLOW_EXIT();
}

/* Create a config object */
int ini_config_create(struct configobj **ini_config)
{
    int error = EOK;
    struct configobj *new_co = NULL;

    TRACE_FLOW_ENTRY();

    if (!ini_config) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    errno = 0;
    new_co = malloc(sizeof(struct configobj));
    if (!new_co) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to allocate memory", ENOMEM);
        return ENOMEM;
    }

    new_co->cfg = NULL;

    /* Create a collection to hold configuration data */
    error = col_create_collection(&(new_co->cfg),
                                  INI_CONFIG_NAME,
                                  COL_CLASS_INI_CONFIG);
    if (error != EOK) {
        TRACE_ERROR_NUMBER("Failed to create collection.", error);
        ini_config_destroy(new_co);
        return error;
    }

    *ini_config = new_co;

    TRACE_FLOW_EXIT();
    return error;
}
