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


/* Check if collision flags are valid */
int valid_collision_flags(uint32_t collision_flags)
{
    uint32_t flag;

    TRACE_FLOW_ENTRY();

    flag = collision_flags & INI_MV1S_MASK;
    if ((flag != INI_MV1S_OVERWRITE) &&
        (flag != INI_MV1S_ERROR) &&
        (flag != INI_MV1S_PRESERVE) &&
        (flag != INI_MV1S_ALLOW) &&
        (flag != INI_MV1S_DETECT)) {
        TRACE_ERROR_STRING("Invalid value collision flag","");
        return 0;
    }

    flag = collision_flags & INI_MV2S_MASK;
    if ((flag != INI_MV2S_OVERWRITE) &&
        (flag != INI_MV2S_ERROR) &&
        (flag != INI_MV2S_PRESERVE) &&
        (flag != INI_MV2S_ALLOW) &&
        (flag != INI_MV2S_DETECT)) {
        TRACE_ERROR_STRING("Invalid value cross-section collision flag","");
        return 0;
    }

    flag = collision_flags & INI_MS_MASK;
    if ((flag != INI_MS_MERGE) &&
        (flag != INI_MS_OVERWRITE) &&
        (flag != INI_MS_ERROR) &&
        (flag != INI_MS_PRESERVE) &&
        (flag != INI_MS_ALLOW) &&
        (flag != INI_MS_DETECT)) {
        TRACE_ERROR_STRING("Invalid section collision flag","");
        return 0;
    }

    TRACE_FLOW_EXIT();
    return 1;
}


/* Close file but not destroy the object */
void ini_config_file_close(struct ini_cfgfile *file_ctx)
{
    TRACE_FLOW_ENTRY();

    if(file_ctx) {
        if(file_ctx->file) {
            fclose(file_ctx->file);
            file_ctx->file = NULL;
        }
    }

    TRACE_FLOW_EXIT();
}

/* Close file context and destroy the object */
void ini_config_file_destroy(struct ini_cfgfile *file_ctx)
{
    TRACE_FLOW_ENTRY();

    if(file_ctx) {
        free(file_ctx->filename);
        col_destroy_collection(file_ctx->error_list);
        if(file_ctx->file) fclose(file_ctx->file);
        free(file_ctx);
    }

    TRACE_FLOW_EXIT();
}

/* Internal common initialization part */
static int common_file_init(struct ini_cfgfile *file_ctx)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    /* Open file */
    TRACE_INFO_STRING("File", file_ctx->filename);
    errno = 0;
    file_ctx->file = fopen(file_ctx->filename, "r");
    if (!(file_ctx->file)) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to open file", error);
        return error;
    }

    /* Create internal collections */
    error = col_create_collection(&(file_ctx->error_list),
                                  INI_ERROR,
                                  COL_CLASS_INI_PERROR);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to create error list", error);
        return error;
    }

    /* Collect stats */
    if (file_ctx->metadata_flags & INI_META_STATS) {
        errno = 0;
        if (fstat(fileno(file_ctx->file),
                  &(file_ctx->file_stats)) < 0) {
            error = errno;
            TRACE_ERROR_NUMBER("Failed to get file stats.", error);
            return error;
        }
    }
    else memset(&(file_ctx->file_stats), 0, sizeof(struct stat));

    TRACE_FLOW_EXIT();
    return EOK;
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

    if (!valid_collision_flags(collision_flags)) {
        TRACE_ERROR_NUMBER("Invalid flags.", EINVAL);
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

    new_ctx->filename = NULL;
    new_ctx->file = NULL;
    new_ctx->error_list = NULL;

    /* Store flags */
    new_ctx->error_level = error_level;
    new_ctx->collision_flags = collision_flags;
    new_ctx->metadata_flags = metadata_flags;
    new_ctx->count = 0;

    /* Construct the full file path */
    errno = 0;
    new_ctx->filename = malloc(PATH_MAX + 1);
    if (!(new_ctx->filename)) {
        error = errno;
        ini_config_file_destroy(new_ctx);
        TRACE_ERROR_NUMBER("Failed to allocate memory for file path.", error);
        return error;
    }

    /* Construct path */
    error = make_normalized_absolute_path(new_ctx->filename,
                                          PATH_MAX,
                                          filename);
    if(error) {
        TRACE_ERROR_NUMBER("Failed to resolve path", error);
        ini_config_file_destroy(new_ctx);
        return error;
    }

    /* Do common init */
    error = common_file_init(new_ctx);
    if(error) {
        TRACE_ERROR_NUMBER("Failed to do common init", error);
        ini_config_file_destroy(new_ctx);
        return error;
    }

    *file_ctx = new_ctx;
    TRACE_FLOW_EXIT();
    return error;
}

/* Create a file object from existing one */
int ini_config_file_reopen(struct ini_cfgfile *file_ctx_in,
                           struct ini_cfgfile **file_ctx_out)
{
    int error = EOK;
    struct ini_cfgfile *new_ctx = NULL;

    TRACE_FLOW_ENTRY();

    if ((!file_ctx_in) || (!file_ctx_out)) {
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

    new_ctx->file = NULL;
    new_ctx->error_list = NULL;

    /* Store flags */
    new_ctx->error_level = file_ctx_in->error_level;
    new_ctx->collision_flags = file_ctx_in->collision_flags;
    new_ctx->metadata_flags = file_ctx_in->metadata_flags;
    new_ctx->count = 0;

    /* Copy full file path */
    errno = 0;
    new_ctx->filename = strndup(file_ctx_in->filename, PATH_MAX);
    if (!(new_ctx->filename)) {
        error = errno;
        ini_config_file_destroy(new_ctx);
        TRACE_ERROR_NUMBER("Failed to allocate memory for file path.", error);
        return error;
    }

    /* Do common init */
    error = common_file_init(new_ctx);
    if(error) {
        TRACE_ERROR_NUMBER("Failed to do common init", error);
        ini_config_file_destroy(new_ctx);
        return error;
    }

    *file_ctx_out = new_ctx;
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
                     ini_get_error_str(pe->error,
                                       INI_FAMILY_PARSING));

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


/* Check access */
int ini_config_access_check(struct ini_cfgfile *file_ctx,
                            uint32_t flags,
                            uid_t uid,
                            gid_t gid,
                            mode_t mode,
                            mode_t mask)
{
    mode_t st_mode;

    TRACE_FLOW_ENTRY();

    flags &= INI_ACCESS_CHECK_MODE |
             INI_ACCESS_CHECK_GID |
             INI_ACCESS_CHECK_UID;

    if ((file_ctx == NULL) || (flags == 0)) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;

    }

    /* Check mode */
    if (flags & INI_ACCESS_CHECK_MODE) {

        TRACE_INFO_NUMBER("File mode as saved.",
                          file_ctx->file_stats.st_mode);

        st_mode = file_ctx->file_stats.st_mode;
        st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;
        TRACE_INFO_NUMBER("File mode adjusted.", st_mode);

        TRACE_INFO_NUMBER("Mode as provided.", mode);
        mode &= S_IRWXU | S_IRWXG | S_IRWXO;
        TRACE_INFO_NUMBER("Mode adjusted.", mode);

        /* Adjust mask */
        if (mask == 0) mask = S_IRWXU | S_IRWXG | S_IRWXO;
        else mask &= S_IRWXU | S_IRWXG | S_IRWXO;

        if ((mode & mask) != (st_mode & mask)) {
            TRACE_INFO_NUMBER("File mode:", (mode & mask));
            TRACE_INFO_NUMBER("Mode adjusted.",
                              (st_mode & mask));
            TRACE_ERROR_NUMBER("Access denied.", EACCES);
            return EACCES;
        }
    }

    /* Check uid */
    if (flags & INI_ACCESS_CHECK_UID) {
        if (file_ctx->file_stats.st_uid != uid) {
            TRACE_ERROR_NUMBER("GID:", file_ctx->file_stats.st_uid);
            TRACE_ERROR_NUMBER("GID passed in.", uid);
            TRACE_ERROR_NUMBER("Access denied.", EACCES);
            return EACCES;
        }
    }

    /* Check gid */
    if (flags & INI_ACCESS_CHECK_GID) {
        if (file_ctx->file_stats.st_gid != gid) {
            TRACE_ERROR_NUMBER("GID:", file_ctx->file_stats.st_gid);
            TRACE_ERROR_NUMBER("GID passed in.", gid);
            TRACE_ERROR_NUMBER("Access denied.", EACCES);
            return EACCES;
        }
    }

    TRACE_FLOW_EXIT();
    return EOK;

}

/* Determines if two file contexts are different by comparing:
 * - time stamp
 * - device ID
 * - i-node
 */
int ini_config_changed(struct ini_cfgfile *file_ctx1,
                       struct ini_cfgfile *file_ctx2,
                       int *changed)
{
    TRACE_FLOW_ENTRY();

    if ((file_ctx1 == NULL) ||
        (file_ctx2 == NULL) ||
        (changed == NULL)) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }

    *changed = 0;

    /* Unfortunately the time is not granular enough
     * to detect the change if run during the unit test.
     * In future when a more granular version of stat
     * is available we should switch to it and update
     * the unit test.
     */

    if((file_ctx1->file_stats.st_mtime !=
        file_ctx2->file_stats.st_mtime) ||
       (file_ctx1->file_stats.st_dev !=
        file_ctx2->file_stats.st_dev) ||
       (file_ctx1->file_stats.st_ino !=
        file_ctx2->file_stats.st_ino)) {
        TRACE_INFO_STRING("File changed!", "");
        *changed = 1;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Print the file object contents */
void ini_config_file_print(struct ini_cfgfile *file_ctx)
{
    TRACE_FLOW_ENTRY();
    if (file_ctx == NULL) {
        printf("No file object\n.");
    }
    else {
        printf("File name: %s\n", (file_ctx->filename) ? file_ctx->filename : "NULL");
        printf("File is %s\n", (file_ctx->file) ? "open" : "closed");
        printf("Error level is %d\n", file_ctx->error_level);
        printf("Collision flags %u\n", file_ctx->collision_flags);
        printf("Metadata flags %u\n", file_ctx->metadata_flags);
        if (file_ctx->error_list) col_print_collection(file_ctx->error_list);
        else printf("Error list is empty.");
        printf("Stats flag st_dev %li\n", file_ctx->file_stats.st_dev);
        printf("Stats flag st_ino %li\n", file_ctx->file_stats.st_ino);
        printf("Stats flag st_mode %u\n", file_ctx->file_stats.st_mode);
        printf("Stats flag st_nlink %li\n", file_ctx->file_stats.st_nlink);
        printf("Stats flag st_uid %u\n", file_ctx->file_stats.st_uid);
        printf("Stats flag st_gid %u\n", file_ctx->file_stats.st_gid);
        printf("Stats flag st_rdev %li\n", file_ctx->file_stats.st_rdev);
        printf("Stats flag st_size %lu\n", file_ctx->file_stats.st_size);
        printf("Stats flag st_blocks %li\n", file_ctx->file_stats.st_blocks);
        printf("Stats flag st_atime %ld\n", file_ctx->file_stats.st_atime);
        printf("Stats flag st_mtime %ld\n", file_ctx->file_stats.st_mtime);
        printf("Stats flag st_ctime %ld\n", file_ctx->file_stats.st_ctime);
        printf("Count %u\n", file_ctx->count);
    }
    TRACE_FLOW_EXIT();
}
