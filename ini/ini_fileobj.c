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

#include "config.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "trace.h"
#include "ini_defines.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "path_utils.h"


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
    new_ctx = malloc(sizeof(struct ini_cfgfile));
    if (!new_ctx) {
        TRACE_ERROR_NUMBER("Failed to allocate file ctx.", ENOMEM);
        return ENOMEM;
    }

    new_ctx->filename = NULL;
    new_ctx->file = NULL;

    /* Store flags */
    new_ctx->metadata_flags = metadata_flags;

    /* Construct the full file path */
    new_ctx->filename = malloc(PATH_MAX + 1);
    if (!(new_ctx->filename)) {
        ini_config_file_destroy(new_ctx);
        TRACE_ERROR_NUMBER("Failed to allocate memory for file path.", ENOMEM);
        return ENOMEM;
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
    new_ctx = malloc(sizeof(struct ini_cfgfile));
    if (!new_ctx) {
        TRACE_ERROR_NUMBER("Failed to allocate file ctx.", ENOMEM);
        return ENOMEM;
    }

    new_ctx->file = NULL;

    /* Store flags */
    new_ctx->metadata_flags = file_ctx_in->metadata_flags;

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
        printf("Metadata flags %u\n", file_ctx->metadata_flags);
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
    }
    TRACE_FLOW_EXIT();
}
