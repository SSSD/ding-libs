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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <iconv.h>
#include "trace.h"
#include "ini_defines.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "path_utils.h"

#define ICONV_BUFFER    5000

#define BOM4_SIZE 4
#define BOM3_SIZE 3
#define BOM2_SIZE 2

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
        simplebuffer_free(file_ctx->file_data);
        if(file_ctx->file) fclose(file_ctx->file);
        free(file_ctx);
    }

    TRACE_FLOW_EXIT();
}

/* How much I plan to read? */
static size_t how_much_to_read(size_t left, size_t increment)
{
    if(left > increment) return increment;
    else return left;
}

static enum index_utf_t check_bom(enum index_utf_t ind,
                                  unsigned char *buffer,
                                  size_t len,
                                  size_t *bom_shift)
{
    TRACE_FLOW_ENTRY();

    if (len >= BOM4_SIZE) {
        if ((buffer[0] == 0x00) &&
            (buffer[1] == 0x00) &&
            (buffer[2] == 0xFE) &&
            (buffer[3] == 0xFF)) {
                TRACE_FLOW_RETURN(INDEX_UTF32BE);
                *bom_shift = BOM4_SIZE;
                return INDEX_UTF32BE;
        }
        else if ((buffer[0] == 0xFF) &&
                 (buffer[1] == 0xFE) &&
                 (buffer[2] == 0x00) &&
                 (buffer[3] == 0x00)) {
                TRACE_FLOW_RETURN(INDEX_UTF32LE);
                *bom_shift = BOM4_SIZE;
                return INDEX_UTF32LE;
        }
    }

    if (len >= BOM3_SIZE) {
        if ((buffer[0] == 0xEF) &&
            (buffer[1] == 0xBB) &&
            (buffer[2] == 0xBF)) {
                TRACE_FLOW_RETURN(INDEX_UTF8);
                *bom_shift = BOM3_SIZE;
                return INDEX_UTF8;
        }
    }

    if (len >= BOM2_SIZE) {
        if ((buffer[0] == 0xFE) &&
            (buffer[1] == 0xFF)) {
                TRACE_FLOW_RETURN(INDEX_UTF16BE);
                *bom_shift = BOM2_SIZE;
                return INDEX_UTF16BE;
        }
        else if ((buffer[0] == 0xFF) &&
                 (buffer[1] == 0xFE)) {
                TRACE_FLOW_RETURN(INDEX_UTF16LE);
                *bom_shift = BOM2_SIZE;
                return INDEX_UTF16LE;
        }
    }

    TRACE_FLOW_RETURN(ind);
    return ind;
}

static int read_chunk(FILE *file, size_t left, size_t increment,
                      char *position, size_t *read_num)
{
    int error = EOK;
    size_t to_read = 0;
    size_t read_cnt = 0;

    TRACE_FLOW_ENTRY();

    to_read = how_much_to_read(left, increment);

    TRACE_INFO_NUMBER("About to read", to_read);

    read_cnt = fread(position, to_read, 1, file);

    TRACE_INFO_NUMBER("Read", read_cnt * to_read);

    if (read_cnt == 0) {
        error = ferror(file);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to read data from file", error);
            return error;
        }
        error = feof(file);
        if(error) {
            TRACE_FLOW_EXIT();
            return EOK;
        }
        TRACE_ERROR_NUMBER("Failed to read data from file", EIO);
        return EIO;
    }

    *read_num = to_read;

    TRACE_FLOW_EXIT();
    return error;
}

/* Function useful for debugging */
/*
static void print_buffer(char *read_buffer, int len)
{
    int i;
    for (i=0; i < len; i++) {
        printf("%02X ", (unsigned char)read_buffer[i]);
    }
    printf("\n");
}
*/

/* Internal initialization part */
static int initialize_conv(unsigned char *read_buf,
                           size_t read_cnt,
                           int *initialized,
                           size_t *bom_shift,
                           iconv_t *conv)
{
    int error = EOK;
    enum index_utf_t ind = INDEX_UTF8;
    const char *encodings[] = {  "UTF-32BE",
                                 "UTF-32LE",
                                 "UTF-16BE",
                                 "UTF-16LE",
                                 "UTF-8" };

    TRACE_FLOW_ENTRY();

    if (*initialized == 0) {

        TRACE_INFO_STRING("Reading first time.","Checking BOM");

        ind = check_bom(ind,
                        (unsigned char *)read_buf,
                        read_cnt,
                        bom_shift);

        TRACE_INFO_STRING("Converting to", encodings[INDEX_UTF8]);
        TRACE_INFO_STRING("Converting from", encodings[ind]);

        errno = 0;
        *conv = iconv_open(encodings[INDEX_UTF8], encodings[ind]);
        if (*conv == (iconv_t) -1) {
            error = errno;
            TRACE_ERROR_NUMBER("Failed to create converter", error);
            return error;
        }

        *initialized = 1;
    }
    else *bom_shift = 0;

    TRACE_FLOW_EXIT();
    return error;

}

/* Internal conversion part */
static int common_file_convert(FILE *file,
                               struct ini_cfgfile *file_ctx,
                               uint32_t size)
{
    int error = EOK;
    size_t read_cnt = 0;
    size_t total_read = 0;
    size_t in_buffer = 0;
    iconv_t conv = (iconv_t)-1;
    size_t conv_res = 0;
    char read_buf[ICONV_BUFFER+1];
    char result_buf[ICONV_BUFFER];
    char *src, *dest;
    size_t to_convert = 0;
    size_t room_left = 0;
    size_t bom_shift = 0;
    int initialized = 0;

    TRACE_FLOW_ENTRY();

    do {
        /* print_buffer(read_buf, ICONV_BUFFER); */
        error = read_chunk(file,
                           size - total_read,
                           ICONV_BUFFER - in_buffer,
                           read_buf + in_buffer,
                           &read_cnt);
        /* print_buffer(read_buf, ICONV_BUFFER); */
        if (error) {
            if (conv != (iconv_t) -1) iconv_close(conv);
            TRACE_ERROR_NUMBER("Failed to read chunk", error);
            return error;
        }

        /* Prepare source buffer for conversion */
        src = read_buf;
        to_convert = read_cnt + in_buffer;
        in_buffer = 0;

        /* Do initialization if needed */
        error = initialize_conv((unsigned char *)read_buf,
                                read_cnt,
                                &initialized,
                                &bom_shift,
                                &conv);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to initialize",
                                error);
            return error;
        }

        src += bom_shift;
        to_convert -= bom_shift;
        total_read += read_cnt;
        TRACE_INFO_NUMBER("Total read", total_read);

        do {
            /* Do conversion */
            dest = result_buf;
            room_left = ICONV_BUFFER;

            TRACE_INFO_NUMBER("To convert", to_convert);
            TRACE_INFO_NUMBER("Room left", room_left);
            TRACE_INFO_NUMBER("Total read", total_read);

            errno = 0;
            conv_res = iconv(conv, &src, &to_convert, &dest, &room_left);
            if (conv_res == (size_t) -1) {
                error = errno;
                switch(error) {
                case EILSEQ:
                    TRACE_ERROR_NUMBER("Invalid multibyte encoding", error);
                    iconv_close(conv);
                    return error;
                case EINVAL:
                    /* We need to just read more if we can */
                    TRACE_INFO_NUMBER("Incomplete sequence len",
                                      src - read_buf);
                    TRACE_INFO_NUMBER("File size.", size);
                    if (total_read == size) {
                        /* Or return error if we can't */
                        TRACE_ERROR_NUMBER("Incomplete sequence", error);
                        iconv_close(conv);
                        return error;
                    }
                    memmove(read_buf, src, to_convert);
                    in_buffer = to_convert;
                    break;

                case E2BIG:
                    TRACE_INFO_STRING("No room in the output buffer.", "");
                    error = simplebuffer_add_raw(file_ctx->file_data,
                                                 result_buf,
                                                 ICONV_BUFFER - room_left,
                                                 ICONV_BUFFER);
                    if (error) {
                        TRACE_ERROR_NUMBER("Failed to store converted bytes",
                                            error);
                        iconv_close(conv);
                        return error;
                    }
                    continue;
                default:
                    TRACE_ERROR_NUMBER("Unexpected internal error",
                                        error);
                    iconv_close(conv);
                    return ENOTSUP;
                }
            }
            /* The whole buffer was sucessfully converted */
            error = simplebuffer_add_raw(file_ctx->file_data,
                                         result_buf,
                                         ICONV_BUFFER - room_left,
                                         ICONV_BUFFER);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to store converted bytes",
                                    error);
                iconv_close(conv);
                return error;
            }
/*
            TRACE_INFO_STRING("Saved procesed portion.",
                        (char *)simplebuffer_get_vbuf(file_ctx->file_data));
*/
            break;
        }
        while (1);
    }
    while (total_read < size);

    iconv_close(conv);

    /* Open file */
    TRACE_INFO_STRING("File data",
                      (char *)simplebuffer_get_vbuf(file_ctx->file_data));
    TRACE_INFO_NUMBER("File len",
                      simplebuffer_get_len(file_ctx->file_data));
    TRACE_INFO_NUMBER("Size", size);
    errno = 0;
    file_ctx->file = fmemopen(simplebuffer_get_vbuf(file_ctx->file_data),
                              simplebuffer_get_len(file_ctx->file_data),
                              "r");
    if (!(file_ctx->file)) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to open file", error);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}


/* Internal common initialization part */
static int common_file_init(struct ini_cfgfile *file_ctx,
                            void *data_buf,
                            uint32_t data_len)
{
    int error = EOK;
    FILE *file = NULL;
    int stat_ret = 0;
    uint32_t size = 0;
    void *internal_data = NULL;
    uint32_t internal_len = 0;
    unsigned char alt_buffer[2] = {0, 0};
    uint32_t alt_buffer_len = 1;

    TRACE_FLOW_ENTRY();

    if (data_buf) {

        if(data_len) {
            internal_data = data_buf;
            internal_len = data_len;
        }
        else {
            /* If buffer is empty fmemopen will return an error.
             * This will prevent creation of adefault config object.
             * Instead we will use buffer that has at least one character. */
            internal_data = alt_buffer;
            internal_len = alt_buffer_len;
        }

        TRACE_INFO_NUMBER("Inside file_init len", internal_len);
        TRACE_INFO_STRING("Inside file_init data:", (char *)internal_data);

        file = fmemopen(internal_data, internal_len, "r");
        if (!file) {
            error = errno;
            TRACE_ERROR_NUMBER("Failed to memmap file", error);
            return error;
        }
        size = internal_len;
    }
    else {

        TRACE_INFO_STRING("File", file_ctx->filename);

        /* Open file to get its size */
        errno = 0;
        file = fopen(file_ctx->filename, "r");
        if (!file) {
            error = errno;
            TRACE_ERROR_NUMBER("Failed to open file", error);
            return error;
        }

        /* Get the size of the file */
        errno = 0;
        stat_ret = fstat(fileno(file), &(file_ctx->file_stats));
        if (stat_ret == -1) {
            error = errno;
            fclose(file);
            TRACE_ERROR_NUMBER("Failed to get file stats", error);
            return error;
        }
        size = file_ctx->file_stats.st_size;
    }

    /* Trick to overcome the fact that
     * fopen and fmemopen behave differently when file
     * is 0 length
     */
    if (size) {
        error = common_file_convert(file, file_ctx, size);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to convert file",
                                error);
            fclose(file);
            return error;
        }
    }
    else {

        TRACE_INFO_STRING("File is 0 length","");
        errno = 0;

        file_ctx->file = fdopen(fileno(file), "r");
        if (!(file_ctx->file)) {
            error = errno;
            fclose(file);
            TRACE_ERROR_NUMBER("Failed to fdopen file", error);
            return error;
        }
    }

    fclose(file);

    /* Collect stats */
    if (file_ctx->metadata_flags & INI_META_STATS) {
        file_ctx->stats_read = 1;
    }
    else {
        memset(&(file_ctx->file_stats), 0, sizeof(struct stat));
        file_ctx->stats_read = 0;
    }

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
    new_ctx->file_data = NULL;

    error = simplebuffer_alloc(&(new_ctx->file_data));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate buffer ctx.", error);
        ini_config_file_destroy(new_ctx);
        return error;

    }

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
    error = common_file_init(new_ctx, NULL, 0);
    if(error) {
        TRACE_ERROR_NUMBER("Failed to do common init", error);
        ini_config_file_destroy(new_ctx);
        return error;
    }

    *file_ctx = new_ctx;
    TRACE_FLOW_EXIT();
    return error;
}

/* Create a file object from a memory buffer */
int ini_config_file_from_mem(void *data_buf,
                             uint32_t data_len,
                             struct ini_cfgfile **file_ctx)
{
    int error = EOK;
    struct ini_cfgfile *new_ctx = NULL;

    TRACE_FLOW_ENTRY();

    if ((!data_buf) || (!file_ctx)) {
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
    new_ctx->file_data = NULL;
    new_ctx->metadata_flags = 0;

    error = simplebuffer_alloc(&(new_ctx->file_data));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate buffer ctx.", error);
        ini_config_file_destroy(new_ctx);
        return error;
    }

    /* Put an empty string into the file name */
    new_ctx->filename = strdup("");
    if (!(new_ctx->filename)) {
        ini_config_file_destroy(new_ctx);
        TRACE_ERROR_NUMBER("Failed to put empty string into filename.", ENOMEM);
        return ENOMEM;
    }

    /* Do common init */
    error = common_file_init(new_ctx, data_buf, data_len);
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
    new_ctx->file_data = NULL;
    new_ctx->filename = NULL;

    error = simplebuffer_alloc(&(new_ctx->file_data));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate buffer ctx.", error);
        ini_config_file_destroy(new_ctx);
        return error;

    }

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
    error = common_file_init(new_ctx, NULL, 0);
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

/* Get pointer to stat structure */
const struct stat *ini_config_get_stat(struct ini_cfgfile *file_ctx)
{
    const struct stat *ret;
    TRACE_FLOW_ENTRY();

    if (file_ctx->stats_read) ret = &(file_ctx->file_stats);
    else ret = NULL;

    TRACE_FLOW_EXIT();
    return ret;
}

/* Check access */
int access_check_int(struct stat *file_stats,
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

    if (flags == 0) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }

    /* Check mode */
    if (flags & INI_ACCESS_CHECK_MODE) {

        TRACE_INFO_NUMBER("File mode as saved.",
                          file_stats->st_mode);

        st_mode = file_stats->st_mode;
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
        if (file_stats->st_uid != uid) {
            TRACE_ERROR_NUMBER("GID:", file_stats->st_uid);
            TRACE_ERROR_NUMBER("GID passed in.", uid);
            TRACE_ERROR_NUMBER("Access denied.", EACCES);
            return EACCES;
        }
    }

    /* Check gid */
    if (flags & INI_ACCESS_CHECK_GID) {
        if (file_stats->st_gid != gid) {
            TRACE_ERROR_NUMBER("GID:", file_stats->st_gid);
            TRACE_ERROR_NUMBER("GID passed in.", gid);
            TRACE_ERROR_NUMBER("Access denied.", EACCES);
            return EACCES;
        }
    }

    TRACE_FLOW_EXIT();
    return EOK;

}

/* Check access */
int ini_config_access_check(struct ini_cfgfile *file_ctx,
                            uint32_t flags,
                            uid_t uid,
                            gid_t gid,
                            mode_t mode,
                            mode_t mask)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if (file_ctx == NULL) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }

    if (file_ctx->stats_read == 0) {
        TRACE_ERROR_NUMBER("Stats were not collected.", EINVAL);
        return EINVAL;
    }

    error =  access_check_int(&(file_ctx->file_stats),
                              flags,
                              uid,
                              gid,
                              mode,
                              mask);

    TRACE_FLOW_EXIT();
    return error;

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

    if ((file_ctx1->stats_read == 0) ||
        (file_ctx2->stats_read == 0)) {
        TRACE_ERROR_NUMBER("Stats were not collected.", EINVAL);
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
