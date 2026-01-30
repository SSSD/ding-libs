/*
    INI LIBRARY

    Header for the internal structures used by INI interface.

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

#ifndef INI_CONFIG_PRIV_H
#define INI_CONFIG_PRIV_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "collection.h"
#include "simplebuffer.h"
#include "ini_comment.h"

/* Configuration object */
struct ini_cfgobj {
    /* For now just a collection */
    struct collection_item *cfg;
    /* Boundary */
    uint32_t boundary;
    /* Last comment */
    struct ini_comment *last_comment;
    /* Last search state */
    char *section;
    char *name;
    int section_len;
    int name_len;
    struct collection_iterator *iterator;
    /* Collection of errors detected during parsing */
    struct collection_item *error_list;
    /* Count of error lines */
    unsigned count;

    /*...         */
    /* Statistics? Timestamps? When created? Modified? - TBD */
    /*...         */
};


/* Configuration file object */
struct ini_cfgfile {
    /***********************************/
    /* Externally controlled variables */
    /***********************************/
    /* File name for the configuration file */
    char *filename;
    /* File stream */
    FILE *file;
    /* What meta data to collect */
    uint32_t metadata_flags;
    /**********************/
    /* Internal variables */
    /**********************/
    /* File stats */
    struct stat file_stats;
    /* Were stats read ? */
    int stats_read;
    /* Internal buffer */
    struct simplebuffer *file_data;
    /* BOM indicator */
    enum index_utf_t bom;
};

/* Parsing error */
struct ini_parse_error {
    unsigned line;
    int error;
};

/* Internal cleanup callback */
void ini_cleanup_cb(const char *property,
                    int property_len,
                    int type,
                    void *data,
                    int length,
                    void *custom_data);

/* Get parsing error */
const char *ini_get_error_str(int parsing_error, int family);

/* Check if collision flags are valid */
int valid_collision_flags(uint32_t collision_flags);

/* Empty section */
int empty_section(struct collection_item *sec);

/* Internal access check function */
int access_check_int(struct stat *file_stats,
                     uint32_t flags,
                     uid_t uid,
                     gid_t gid,
                     mode_t mode,
                     mode_t mask);

/**
 * @brief Serialize configuration object
 *
 * Serialize configuration object into provided buffer.
 * Use buffer object functions to manipulate or save
 * the buffer to a file/stream.
 *
 * @param[in]  ini_config       Configuration object.
 * @param[out] sbobj            Serialized configuration.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_serialize(struct ini_cfgobj *ini_config,
                         struct simplebuffer *sbobj);

/* Serialize value */
int value_serialize(struct value_obj *vo,
                    const char *key,
                    struct simplebuffer *sbobj);

/* Serialize comment */
int ini_comment_serialize(struct ini_comment *ic,
                          struct simplebuffer *sbobj);

struct ini_errmsg;

struct ini_errobj {
    size_t count;
    struct ini_errmsg *first_msg;
    struct ini_errmsg *last_msg;
    struct ini_errmsg *cur_msg;
};

struct ini_errmsg {
    char *str;
    struct ini_errmsg *next;
};

#endif
