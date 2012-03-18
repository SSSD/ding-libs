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

/* Configuration object */
struct ini_cfgobj {
    /* For now just a collection */
    struct collection_item *cfg;
    /* Boundary */
    uint32_t boundary;
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
    /* Error level */
    int error_level;
    /* Collision flags - define how to merge things */
    uint32_t collision_flags;
    /* What meta data to collect */
    uint32_t metadata_flags;
    /**********************/
    /* Internal variables */
    /**********************/
    /* Collection of errors detected during parsing */
    struct collection_item *error_list;
    /* File stats */
    struct stat file_stats;
    /* Count of error lines */
    unsigned count;
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

#endif
