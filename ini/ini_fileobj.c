/*
    INI LIBRARY

    File context related functions

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
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "trace.h"
#include "ini_defines.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "collection.h"
#include "path_utils.h"
#include "collection_tools.h"


/* Close file context and destroy the object */
void ini_config_file_close(struct ini_cfgfile *file_ctx)
{
    TRACE_FLOW_ENTRY();

    if(file_ctx) {
        free(file_ctx->filename);
        col_destroy_collection(file_ctx->error_list);
        col_destroy_collection(file_ctx->metadata);
        fclose(file_ctx->file);
        free(file_ctx);
    }

    TRACE_FLOW_EXIT();
}

/* Create a file object for parsing a config file */
int ini_config_file_open(const char *filename,
                         int error_level,
                         uint32_t collision_flags,
                         uint32_t metadata_flags,
                         struct ini_cfgfile **file_ctx)
{
    int error = EOK;
    struct ini_cfgfile *new_ctx = NULL;

    TRACE_FLOW_ENTRY();

    if ((!filename) || (!file_ctx)) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }

    /* Allocate structure */
    errno = 0;
    new_ctx = malloc(sizeof(struct ini_cfgfile));
    if (!new_ctx) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to allocate file ctx.", error);
        return error;
    }

    /* Construct the full file path */
    errno = 0;
    new_ctx->filename = malloc(PATH_MAX + 1);
    if (!(new_ctx->filename)) {
        error = errno;
        ini_config_file_close(new_ctx);
        TRACE_ERROR_NUMBER("Failed to allocate memory for file path.", error);
        return error;
    }

    /* Construct path */
    error = make_normalized_absolute_path(new_ctx->filename,
                                          PATH_MAX,
                                          filename);
    if(error) {
        TRACE_ERROR_NUMBER("Failed to resolve path", error);
        ini_config_file_close(new_ctx);
        return error;
    }

    /* Open file */
    TRACE_INFO_STRING("File", new_ctx->filename);
    errno = 0;
    new_ctx->file = NULL;
    new_ctx->file = fopen(new_ctx->filename, "r");
    if (!(new_ctx->file)) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to open file", error);
        ini_config_file_close(new_ctx);
        return error;
    }

    /* Store flags */
    new_ctx->error_level = error_level;
    new_ctx->collision_flags = collision_flags;
    new_ctx->metadata_flags = metadata_flags;
    new_ctx->count = 0;

    /* Create internal collections */
    error = col_create_collection(&(new_ctx->error_list),
                                  INI_ERROR,
                                  COL_CLASS_INI_PERROR);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to create error list", error);
        ini_config_file_close(new_ctx);
        return error;
    }

    error = col_create_collection(&(new_ctx->metadata),
                                  INI_METADATA,
                                  COL_CLASS_INI_META);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to create metadata collection", error);
        ini_config_file_close(new_ctx);
        return error;
    }

    *file_ctx = new_ctx;
    TRACE_FLOW_EXIT();
    return error;
}


/* How many errors do we have in the list ? */
unsigned ini_config_error_count(struct ini_cfgfile *file_ctx)
{
    unsigned count = 0;

    TRACE_FLOW_ENTRY();

    count = file_ctx->count;

    TRACE_FLOW_EXIT();
    return count;

}

/* Free error strings */
void ini_config_free_errors(char **errors)
{
    TRACE_FLOW_ENTRY();

    col_free_property_list(errors);

    TRACE_FLOW_EXIT();
}

/* Get the list of error strings */
int ini_config_get_errors(struct ini_cfgfile *file_ctx,
                          char ***errors)
{
    char **errlist = NULL;
    struct collection_iterator *iterator = NULL;
    int error;
    struct collection_item *item = NULL;
    struct ini_parse_error *pe;
    unsigned int count = 0;
    char *line;

    TRACE_FLOW_ENTRY();

    /* If we have something to print print it */
    if ((!errors) || (!file_ctx)) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }


    errno = 0;
    errlist = calloc(file_ctx->count + 1, sizeof(char *));
    if (!errlist) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to allocate memory for errors.", error);
        return error;
    }

    /* Bind iterator */
    error =  col_bind_iterator(&iterator,
                               file_ctx->error_list,
                               COL_TRAVERSE_DEFAULT);
    if (error) {
        TRACE_ERROR_NUMBER("Faile to bind iterator:", error);
        ini_config_free_errors(errlist);
        return error;
    }

    while(1) {
        /* Loop through a collection */
        error = col_iterate_collection(iterator, &item);
        if (error) {
            TRACE_ERROR_NUMBER("Error iterating collection", error);
            col_unbind_iterator(iterator);
            ini_config_free_errors(errlist);
            return error;
        }

        /* Are we done ? */
        if (item == NULL) break;

        /* Process collection header */
        if (col_get_item_type(item) == COL_TYPE_COLLECTION) {
            continue;
        }
        else {
            /* Put error into provided format */
            pe = (struct ini_parse_error *)(col_get_item_data(item));

            /* Would be nice to have asprintf function...
             * ...but for now we know that all the errors
             * are pretty short and will fir into the predefined
             * error length buffer.
             */
            errno = 0;
            line = malloc(MAX_ERROR_LINE + 1);
            if (!line) {
                error = errno;
                TRACE_ERROR_NUMBER("Failed to get memory for error.", error);
                col_unbind_iterator(iterator);
                ini_config_free_errors(errlist);
                return error;
            }

            snprintf(line, MAX_ERROR_LINE, LINE_FORMAT,
                     col_get_item_property(item, NULL),
                     pe->error,
                     pe->line,
                     parsing_error_str(pe->error));

            errlist[count] = line;
            count++;
        }

    }

    /* Do not forget to unbind iterator - otherwise there will be a leak */
    col_unbind_iterator(iterator);

    *errors = errlist;

    TRACE_FLOW_EXIT();
    return error;
}

/* Get the fully resolved file name */
const char *ini_config_get_filename(struct ini_cfgfile *file_ctx)
{
    const char *ret;
    TRACE_FLOW_ENTRY();

    ret = file_ctx->filename;

    TRACE_FLOW_EXIT();
    return ret;
}
