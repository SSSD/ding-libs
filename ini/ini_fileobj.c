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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <iconv.h>
#include <dirent.h>
#include "trace.h"
#include "ini_defines.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "path_utils.h"

#define ICONV_BUFFER    5000

#define BOM4_SIZE 4
#define BOM3_SIZE 3
#define BOM2_SIZE 2

static const char *encodings[] = { "UTF-32BE",
                                   "UTF-32LE",
                                   "UTF-16BE",
                                   "UTF-16LE",
                                   "UTF-8",
                                   "UTF-8" };

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
                           enum index_utf_t *in_ind,
                           iconv_t *conv)
{
    int error = EOK;
    enum index_utf_t ind = INDEX_UTF8NOBOM;

    TRACE_FLOW_ENTRY();

    if (*initialized == 0) {

        TRACE_INFO_STRING("Reading first time.","Checking BOM");

        ind = check_bom(ind,
                        (unsigned char *)read_buf,
                        read_cnt,
                        bom_shift);

        TRACE_INFO_STRING("Converting to", encodings[INDEX_UTF8NOBOM]);
        TRACE_INFO_STRING("Converting from", encodings[ind]);

        errno = 0;
        *conv = iconv_open(encodings[INDEX_UTF8], encodings[ind]);
        if (*conv == (iconv_t) -1) {
            error = errno;
            TRACE_ERROR_NUMBER("Failed to create converter", error);
            return error;
        }

        *initialized = 1;
        *in_ind = ind;
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
    enum index_utf_t ind = INDEX_UTF8NOBOM;

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
                                &ind,
                                &conv);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to initialize",
                                error);
            return error;
        }

        src += bom_shift;
        to_convert -= bom_shift;
        total_read += read_cnt;
        file_ctx->bom = ind;
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
    new_ctx->bom = INDEX_UTF8NOBOM;

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
    new_ctx->bom = INDEX_UTF8NOBOM;

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

    new_ctx->bom = file_ctx_in->bom;

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

/* Function to construct file name */
static int create_file_name(const char *dir,
                            const char *tpl,
                            unsigned count,
                            char **filename)
{
    char *resolved = NULL;
    char *full_name = NULL;
    int ret = 0;
    const char *dir_to_use;
    char dirbuf[PATH_MAX * 2 + 1];

    TRACE_FLOW_ENTRY();

    /* We checked the template so it should be safe */
    ret = asprintf(&resolved, tpl, count);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("First asprintf falied.", ENOMEM);
        return ENOMEM;
    }

    /* If directory is not provided use current */
    if (dir) dir_to_use = dir;
    else {
        memset(dirbuf, 0 , PATH_MAX * 2 + 1);
        dir_to_use = getcwd(dirbuf, PATH_MAX * 2);
    }

    ret = asprintf(&full_name, "%s/%s", dir_to_use, resolved);
    free(resolved);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Second asprintf falied.", ENOMEM);
        return ENOMEM;
    }

    *filename = full_name;

    TRACE_FLOW_EXIT();
    return EOK;
}


/* Function to determine which permissions to use */
static int determine_permissions(struct ini_cfgfile *file_ctx,
                                 struct access_check *overwrite,
                                 uid_t *uid_ptr,
                                 gid_t *gid_ptr,
                                 mode_t *mode_ptr)
{
    int error = EOK;
    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = 0;
    struct stat stats;
    int ret = 0;

    TRACE_FLOW_ENTRY();

    /* Prepare default uid, gid, mode */
    if (file_ctx->stats_read) {
        uid = file_ctx->file_stats.st_uid;
        gid = file_ctx->file_stats.st_gid;
        mode = file_ctx->file_stats.st_mode;
    }
    else if (*(file_ctx->filename) != '\0') {
        /* If file name is known check the file */
        memset(&stats, 0, sizeof(struct stat));
        ret = stat(file_ctx->filename, &stats);
        if (ret == -1) {
            error = errno;
            TRACE_ERROR_NUMBER("Stat falied.", error);
            return error;
        }
        uid = stats.st_uid;
        gid = stats.st_gid;
        mode = stats.st_mode;
    }
    else {
        /* Use process properties */
        uid = geteuid();
        gid = getegid();
        /* Regular file that can be read or written by owner only */
        mode = S_IRUSR | S_IWUSR;
    }

    /* If caller specified "overwrite" data overwrite the defaults */
    if (overwrite) {

        overwrite->flags &= INI_ACCESS_CHECK_MODE |
                            INI_ACCESS_CHECK_GID |
                            INI_ACCESS_CHECK_UID;

        if (overwrite->flags == 0) {
            TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
            return EINVAL;
        }

        /* Mode is specified */
        if (overwrite->flags & INI_ACCESS_CHECK_MODE) {
            mode = overwrite->mode;
        }

        /* Check uid */
        if (overwrite->flags & INI_ACCESS_CHECK_UID) {
            uid = overwrite->uid;
        }

        /* Check gid */
        if (overwrite->flags & INI_ACCESS_CHECK_GID) {
            gid = overwrite->gid;
        }
    }

    *uid_ptr = uid;
    *gid_ptr = gid;
    *mode_ptr = mode;

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Create file and set proper permissions */
static int open_new_file(const char *filename,
                         uid_t uid,
                         gid_t gid,
                         mode_t mode,
                         int check,
                         int *fd_ptr)
{
    int error = EOK;
    int ret = 0;
    int fd;

    TRACE_FLOW_ENTRY();

    if (check) {
        errno = 0;
        fd = open(filename, O_RDONLY);
        if (fd != -1) {
            close(fd);
            TRACE_ERROR_NUMBER("File already exists.", error);
            return EEXIST;
        }
        else {
            error = errno;
            if (error == EACCES) {
                TRACE_ERROR_NUMBER("Failed to open file.", error);
                return error;
            }
        }
    }

    /* Keep in mind that umask of the process has impactm, see man pages. */
    errno = 0;
    fd = creat(filename, mode);
    if (fd == -1) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to create file.", error);
        return error;
    }

    errno = 0;
    ret = fchmod(fd, mode);
    if (ret == -1) {
        error = errno;
        close(fd);
        TRACE_ERROR_NUMBER("Failed to chmod file.", error);
        return error;
    }

    errno = 0;
    ret = fchown(fd, uid, gid);
    if (ret == -1) {
        error = errno;
        close(fd);
        TRACE_ERROR_NUMBER("Failed to chown file.", error);
        return error;
    }

    *fd_ptr = fd;

    TRACE_FLOW_EXIT();
    return EOK;

}

/* Function to do the encoding */
static int do_encoding(struct ini_cfgfile *file_ctx,
                       struct simplebuffer *sb)
{
    int error = EOK;
    iconv_t encoder;
    char *src, *dest;
    size_t to_convert = 0;
    size_t room_left = 0;
    char result_buf[ICONV_BUFFER];
    size_t conv_res = 0;

    TRACE_FLOW_ENTRY();

    encoder = iconv_open(encodings[file_ctx->bom], encodings[INDEX_UTF8NOBOM]);
    if (encoder == (iconv_t) -1) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to create converter", error);
        return error;
    }

    src = (char *)simplebuffer_get_vbuf(file_ctx->file_data);
    to_convert = (size_t)simplebuffer_get_len(file_ctx->file_data);

    do {
        /* There is only one loop since everything is already read.
         * We loop only if output buffer is not enough. */

        dest = result_buf;
        room_left = ICONV_BUFFER;

        errno = 0;
        conv_res = iconv(encoder, &src, &to_convert, &dest, &room_left);
        if (conv_res == (size_t) -1) {
            error = errno;
            switch(error) {
            case EILSEQ:
                TRACE_ERROR_NUMBER("Invalid multibyte encoding", error);
                iconv_close(encoder);
                return error;
            case EINVAL:
                TRACE_ERROR_NUMBER("Incomplete sequence", error);
                iconv_close(encoder);
                return error;
            case E2BIG:
                TRACE_INFO_STRING("No room in the output buffer.", "");
                error = simplebuffer_add_raw(sb,
                                             result_buf,
                                             ICONV_BUFFER - room_left,
                                             ICONV_BUFFER);
                if (error) {
                    TRACE_ERROR_NUMBER("Failed to store converted bytes",
                                        error);
                    iconv_close(encoder);
                    return error;
                }
                continue;
            default:
                TRACE_ERROR_NUMBER("Unexpected internal error",
                                    error);
                iconv_close(encoder);
                return ENOTSUP;
            }
        }

        /* The whole buffer was sucessfully converted */
        error = simplebuffer_add_raw(sb,
                                     result_buf,
                                     ICONV_BUFFER - room_left,
                                     ICONV_BUFFER);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to store converted bytes",
                                error);
            iconv_close(encoder);
            return error;
        }
/*
        TRACE_INFO_STRING("Saved procesed portion.",
                    (char *)simplebuffer_get_vbuf(sb));
*/
        break;

    }
    while(1);

    iconv_close(encoder);
    TRACE_FLOW_EXIT();
    return EOK;
}

/* Function to do the encoding */
static int write_bom(int fd,
                     enum index_utf_t bom)
{
    unsigned char buffer[4];
    size_t size = 0;
    ssize_t ret;
    int error = EOK;

    TRACE_FLOW_ENTRY();

    switch (bom) {

    case INDEX_UTF32BE:
            buffer[0] = 0x00;
            buffer[1] = 0x00;
            buffer[2] = 0xFE;
            buffer[3] = 0xFF;
            size = BOM4_SIZE;
            break;

    case INDEX_UTF32LE:
            buffer[0] = 0xFF;
            buffer[1] = 0xFE;
            buffer[2] = 0x00;
            buffer[3] = 0x00;
            size = BOM4_SIZE;
            break;

    case INDEX_UTF8:
            buffer[0] = 0xEF;
            buffer[1] = 0xBB;
            buffer[2] = 0xBF;
            size = BOM3_SIZE;
            break;

    case INDEX_UTF16BE:
            buffer[0] = 0xFE;
            buffer[1] = 0xFF;
            size = BOM2_SIZE;
            break;

    case INDEX_UTF16LE:
            buffer[0] = 0xFF;
            buffer[1] = 0xFE;
            size = BOM2_SIZE;
            break;

    default:
            /* Should not happen - but then size will be 0 and
             * nothing will be written*/
            TRACE_ERROR_NUMBER("Invalid bom type", bom);
            break;
    }

    ret = write(fd, buffer, size);
    if (ret == -1) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to write bom bytes.", error);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Function to write to file */
static int write_to_file(struct ini_cfgfile *file_ctx,
                         const char *filename,
                         struct access_check *overwrite,
                         int check)
{
    int error = EOK;
    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = 0;
    int fd = -1;
    uint32_t left = 0;
    struct simplebuffer *sb = NULL;
    struct simplebuffer *sb_ptr = NULL;

    TRACE_FLOW_ENTRY();

    /* Determine which permissions and ownership to use */
    error = determine_permissions(file_ctx,
                                  overwrite,
                                  &uid,
                                  &gid,
                                  &mode);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to determine permissions.", error);
        return error;
    }

    /* Open file and set proper permissions and ownership */
    error = open_new_file(filename,
                          uid,
                          gid,
                          mode,
                          check,
                          &fd);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to open new file.", error);
        return error;
    }

    /* Write to file */
    if (file_ctx->bom != INDEX_UTF8NOBOM) {

        if (file_ctx->bom != INDEX_UTF8) {

            error = simplebuffer_alloc(&sb);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to allocate buffer for conversion",
                                   error);
                close(fd);
                return error;
            }

            /* Convert buffer */
            error = do_encoding(file_ctx, sb);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to re-encode", error);
                simplebuffer_free(sb); /* Checks for NULL */
                close(fd);
                return error;
            }
            sb_ptr = sb;

        }
        else sb_ptr = file_ctx->file_data;

        /* Write bom into file */
        error = write_bom(fd, file_ctx->bom);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to save bom", error);
            simplebuffer_free(sb); /* Checks for NULL */
            close(fd);
            return error;
        }

    }
    else sb_ptr = file_ctx->file_data;

    left = simplebuffer_get_len(sb_ptr);
    do {
        error = simplebuffer_write(fd, sb_ptr, &left);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to write data", error);
            simplebuffer_free(sb); /* Checks for NULL */
            close(fd);
            return error;
        }
    }
    while (left > 0);

    simplebuffer_free(sb); /* Checks for NULL */
    close(fd);

    TRACE_FLOW_EXIT();
    return error;
}

/* Function to check the template
 * Template is allowed to have '%%' as many times  and caller wants
 * but only one %d. No other combination with a percent is allowed.
 */
static int check_template(const char *tpl)
{
    char *ptr;
    char *ptr_pcnt = NULL;

    TRACE_FLOW_ENTRY();

    /* To be able to scan const char we need a non const pointer */
    ptr = (char *)(intptr_t)tpl;

    for (;;) {
        /* Find first % */
        ptr = strchr(ptr, '%');
        if (ptr == NULL) {
            TRACE_ERROR_NUMBER("No '%%d' found in format", EINVAL);
            return EINVAL;
        }
        else { /* Found */
            if (*(ptr + 1) == 'd') {
                ptr_pcnt = ptr + 2;
                /* We got a valid %d. Check the rest of the string. */
                for (;;) {
                    ptr_pcnt = strchr(ptr_pcnt, '%');
                    if (ptr_pcnt) {
                        ptr_pcnt++;
                        if (*ptr_pcnt != '%') {
                            TRACE_ERROR_NUMBER("Single '%%' "
                                               "symbol after '%%d'.", EINVAL);
                            return EINVAL;
                        }
                        ptr_pcnt++;
                    }
                    else break;
                }
                break;
            }
            /* This is %% - skip */
            else if (*(ptr + 1) == '%') {
                ptr += 2;
                continue;
            }
            else {
                TRACE_ERROR_NUMBER("Single '%%' "
                                   "symbol before '%%d'.", EINVAL);
                return EINVAL;
            }
        }
    }
    TRACE_FLOW_EXIT();
    return EOK;
}

/* Backup a file */
int ini_config_file_backup(struct ini_cfgfile *file_ctx,
                           const char *backup_dir,
                           const char *backup_tpl,
                           struct access_check *backup_access,
                           unsigned max_num)
{
    int error = EOK;
    DIR *ddir = NULL;
    char *filename = NULL;
    unsigned i;

    TRACE_FLOW_ENTRY();

    if (file_ctx == NULL) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }

    if (backup_tpl == NULL) {
        TRACE_ERROR_NUMBER("Name template is required.", EINVAL);
        return EINVAL;
    }

    /* Check the template */
    error = check_template(backup_tpl);
    if (error) {
        TRACE_ERROR_NUMBER("Name template is invalid.", error);
        return error;
    }

    if (backup_dir) {
        /* Check that directory exists */
        errno = 0;
        ddir = opendir(backup_dir);
        if (!ddir) {
            error = errno;
            TRACE_ERROR_NUMBER("Something is wrong with the directory.", error);
            return error;
        }
    }

    for (i = 1; i <= max_num; i++) {

        error = create_file_name(backup_dir, backup_tpl, i, &filename);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to create path.", error);
            if (ddir) closedir(ddir);
            return error;
        }

        error = write_to_file(file_ctx, filename, backup_access, 1);
        free(filename);
        if (error) {
            if ((error == EEXIST) || (error == EACCES)) {
                /* There is a file that already exists,
                 * we need to retry.
                 */
                TRACE_INFO_STRING("File existis.", "Retrying.");
                continue;
            }
            TRACE_ERROR_NUMBER("Failed to write file.", error);
            if (ddir) closedir(ddir);
            return error;
        }
        break;
    }

    if (ddir) closedir(ddir);
    TRACE_FLOW_EXIT();
    return error;
}

/* Change access and ownership */
int ini_config_change_access(struct ini_cfgfile *file_ctx,
                             struct access_check *new_access)
{
    int error = EOK;
    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = 0;
    int ret;

    TRACE_FLOW_ENTRY();

    /* Check that file has name */
    if (*(file_ctx->filename) == '\0') {
        TRACE_ERROR_NUMBER("Invalid file context.", EINVAL);
        return EINVAL;
    }

    if (!(new_access)) {
        TRACE_ERROR_NUMBER("Access structure is required.", EINVAL);
        return EINVAL;
    }

    /* Determine which permissions and ownership to use */
    error = determine_permissions(file_ctx,
                                  new_access,
                                  &uid,
                                  &gid,
                                  &mode);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to determine permissions.", error);
        return error;
    }

    errno = 0;
    ret = chmod(file_ctx->filename, mode);
    if (ret == -1) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to chmod file.", error);
        return error;
    }

    errno = 0;
    ret = chown(file_ctx->filename, uid, gid);
    if (ret == -1) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to chown file.", error);
        return error;
    }

    if (file_ctx->metadata_flags & INI_META_STATS) {
        file_ctx->stats_read = 1;
        ret = stat(file_ctx->filename, &(file_ctx->file_stats));
        if (ret == -1) {
            error = errno;
            TRACE_ERROR_NUMBER("Failed to get file stats", error);
            return error;
        }
    }
    else {
        memset(&(file_ctx->file_stats), 0, sizeof(struct stat));
        file_ctx->stats_read = 0;
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Save configuration in a file */
int ini_config_save(struct ini_cfgfile *file_ctx,
                    struct access_check *new_access,
                    struct ini_cfgobj *ini_config)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_config_save_as(file_ctx,
                               NULL,
                               new_access,
                               ini_config);

    TRACE_FLOW_EXIT();
    return error;
}

/* Save configuration in a file using existing context but with a new name */
int ini_config_save_as(struct ini_cfgfile *file_ctx,
                       const char *filename,
                       struct access_check *new_access,
                       struct ini_cfgobj *ini_config)
{
    int error = EOK;
    struct simplebuffer *sbobj = NULL;

    TRACE_FLOW_ENTRY();

    if (*(file_ctx->filename) == '\0') {
        TRACE_ERROR_NUMBER("Attempt to use wrong file context", EINVAL);
        return EINVAL;
    }

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate buffer.", error);
        return error;
    }

    error = ini_config_serialize(ini_config, sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to serialize.", error);
        simplebuffer_free(sbobj);
        return error;
    }

    /* Close the internal file handle we control */
    ini_config_file_close(file_ctx);

    /* Free old buffer and assign a new one */
    simplebuffer_free(file_ctx->file_data);
    file_ctx->file_data = sbobj;
    sbobj = NULL;

    if (filename) {
        /* Clean existing file name */
        free(file_ctx->filename);
        file_ctx->filename = NULL;

        /* Allocate new */
        file_ctx->filename = malloc(PATH_MAX + 1);
        if (!(file_ctx->filename)) {
            TRACE_ERROR_NUMBER("Failed to allocate memory for file path.",
                               ENOMEM);
            return ENOMEM;
        }

        /* Construct path */
        error = make_normalized_absolute_path(file_ctx->filename,
                                              PATH_MAX,
                                              filename);
        if(error) {
            TRACE_ERROR_NUMBER("Failed to resolve path", error);
            return error;
        }
    }

    /* Write the buffer we have to the file */
    error = write_to_file(file_ctx, file_ctx->filename, new_access, 0);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to write file.", error);
        return error;
    }

    /* Free again to truncate and prepare for re-read */
    simplebuffer_free(file_ctx->file_data);
    file_ctx->file_data = NULL;

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate buffer.", error);
        return error;
    }

    file_ctx->file_data = sbobj;

    /* Reopen and re-read */
    error = common_file_init(file_ctx, NULL, 0);
    if(error) {
        TRACE_ERROR_NUMBER("Failed to do common init", error);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Get the BOM type */
enum index_utf_t ini_config_get_bom(struct ini_cfgfile *file_ctx)
{
    enum index_utf_t ret;
    TRACE_FLOW_ENTRY();

    ret = file_ctx->bom;

    TRACE_FLOW_EXIT();
    return ret;
}


/* Set the BOM type */
int ini_config_set_bom(struct ini_cfgfile *file_ctx, enum index_utf_t bom)
{
    TRACE_FLOW_ENTRY();

    if (file_ctx == NULL) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }

    file_ctx->bom = bom;

    TRACE_FLOW_EXIT();
    return EOK;
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
            TRACE_ERROR_NUMBER("UID:", file_stats->st_uid);
            TRACE_ERROR_NUMBER("UID passed in.", uid);
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
        printf("File name: %s\n",
               (file_ctx->filename) ? file_ctx->filename : "NULL");
        printf("File is %s\n", (file_ctx->file) ? "open" : "closed");
        printf("File BOM %d\n", file_ctx->bom);
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
