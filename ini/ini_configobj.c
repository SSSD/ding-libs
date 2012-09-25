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

/* Clean the search state */
void ini_config_clean_state(struct ini_cfgobj *ini_config)
{
    TRACE_FLOW_ENTRY();

    if (ini_config) {
        if (ini_config->iterator) col_unbind_iterator(ini_config->iterator);
        ini_config->iterator = NULL;
        free(ini_config->section);
        ini_config->section = NULL;
        free(ini_config->name);
        ini_config->name = NULL;
        ini_config->section_len = 0;
        ini_config->name_len = 0;
    }

    TRACE_FLOW_EXIT();
}



/* Traverse the collection and clean the object */
void ini_config_destroy(struct ini_cfgobj *ini_config)
{
    TRACE_FLOW_ENTRY();

    ini_config_clean_state(ini_config);

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
int ini_config_create(struct ini_cfgobj **ini_config)
{
    int error = EOK;
    struct ini_cfgobj *new_co = NULL;

    TRACE_FLOW_ENTRY();

    if (!ini_config) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    errno = 0;
    new_co = malloc(sizeof(struct ini_cfgobj));
    if (!new_co) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to allocate memory", ENOMEM);
        return ENOMEM;
    }

    new_co->cfg = NULL;
    new_co->boundary = INI_WRAP_BOUNDARY;
    new_co->section = NULL;
    new_co->name = NULL;
    new_co->section_len = 0;
    new_co->name_len = 0;
    new_co->iterator = NULL;

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

/* Callback to set the boundary */
int ini_boundary_cb(const char *property,
                     int property_len,
                     int type,
                     void *data,
                     int length,
                     void *custom_data,
                     int *dummy)
{
    int error = EOK;
    struct value_obj *vo = NULL;
    uint32_t boundary;

    TRACE_FLOW_ENTRY();

    boundary = *((uint32_t *)(custom_data));
    /* Banary items are the values */
    if(type == COL_TYPE_BINARY) {
        vo = *((struct value_obj **)(data));
        error = value_set_boundary(vo, boundary);
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Set the folding boundary for multiline values.
 * Use before serializing and saving to a file if the
 * default boundary of 80 characters does not work for you.
 */
int ini_config_set_wrap(struct ini_cfgobj *ini_config,
                        uint32_t boundary)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if (!ini_config) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    ini_config->boundary = boundary;
    error = col_traverse_collection(ini_config->cfg,
                                    COL_TRAVERSE_DEFAULT,
                                    ini_boundary_cb,
                                    (void *)(&(ini_config->boundary)));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to set wrapping boundary", error);
        return error;
    }


    TRACE_FLOW_EXIT();
    return error;
}

/* Configuration copy callback */
static int ini_copy_cb(struct collection_item *item,
                       void *ext_data,
                       int *skip)
{
    int error = EOK;
    struct value_obj *vo = NULL;
    struct value_obj *new_vo = NULL;

    TRACE_FLOW_ENTRY();

    *skip = 0;

    /* Banary items are the values */
    if(col_get_item_type(item) == COL_TYPE_BINARY) {
        vo = *((struct value_obj **)(col_get_item_data(item)));

        error = value_copy(vo, &new_vo);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to copy value", error);
            return error;
        }

        error = col_modify_binary_item(item,
                                       NULL,
                                       &new_vo,
                                       sizeof(struct value_obj *));
        if (error) {
            TRACE_ERROR_NUMBER("Failed to copy value", error);
            value_destroy(new_vo);
            return error;
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Copy configuration */
int ini_config_copy(struct ini_cfgobj *ini_config,
                    struct ini_cfgobj **ini_new)
{
    int error = EOK;
    struct ini_cfgobj *new_co = NULL;

    TRACE_FLOW_ENTRY();

    if ((!ini_config) ||
        (!ini_new)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    /* Create a new configuration object */
    errno = 0;
    new_co = malloc(sizeof(struct ini_cfgobj));
    if (!new_co) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to allocate memory", ENOMEM);
        return ENOMEM;
    }

    new_co->cfg = NULL;
    new_co->boundary = ini_config->boundary;
    new_co->section = NULL;
    new_co->name = NULL;
    new_co->section_len = 0;
    new_co->name_len = 0;
    new_co->iterator = NULL;

    error = col_copy_collection_with_cb(&(new_co->cfg),
                                        ini_config->cfg,
                                        INI_CONFIG_NAME,
                                        COL_COPY_NORMAL,
                                        ini_copy_cb,
                                        NULL);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to copy collection", error);
        ini_config_destroy(new_co);
        return error;
    }

    *ini_new = new_co;

    TRACE_FLOW_EXIT();
    return error;
}
