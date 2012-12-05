/*
    INI LIBRARY

    Module contains functions to serialize configuration object.

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
#include "simplebuffer.h"
#include "collection.h"
#include "ini_valueobj.h"
#include "ini_defines.h"
#include "ini_config_priv.h"
#include "trace.h"

/* Callback */
static int ini_serialize_cb(const char *property,
                            int property_len,
                            int type,
                            void *data,
                            int length,
                            void *custom_data,
                            int *stop)
{
    int error = EOK;
    struct simplebuffer *sbobj;
    struct value_obj *vo;

    TRACE_FLOW_ENTRY();

    /* Banary items are the values */
    if(type == COL_TYPE_BINARY) {
        sbobj = (struct simplebuffer *)custom_data;
        vo = *((struct value_obj **)(data));
        error = value_serialize(vo, property, sbobj);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to serizlize value", error);
            *stop = 1;
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Traverse the collection and build the serialization object */
int ini_config_serialize(struct ini_cfgobj *ini_config,
                         struct simplebuffer *sbobj)
{
    int error = EOK;
    TRACE_FLOW_ENTRY();

    if (!ini_config) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    if (ini_config->cfg) {
        error = col_traverse_collection(ini_config->cfg,
                                        COL_TRAVERSE_DEFAULT,
                                        ini_serialize_cb,
                                        (void *)sbobj);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to serialize collection", error);
            return error;
        }
    }

    if (ini_config->last_comment) {
        error = ini_comment_serialize(ini_config->last_comment, sbobj);
        if (error) {
            TRACE_ERROR_NUMBER("Failed serialize comment", error);
            return error;
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}
