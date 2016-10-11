/*
    INI LIBRARY

    Header file for the internal constants for the INI interface.

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

#ifndef INI_DEFINES_H
#define INI_DEFINES_H

#define NAME_OVERHEAD   10

#define SLASH           "/"


/* Name of the special collection used to store parsing errors */
#define FILE_ERROR_SET  "ini_file_error_set"

/* Text error strings used when errors are printed out */
#define WARNING_TXT         _("Warning")
#define ERROR_TXT           _("Error")
/* For parse errors */
#define WRONG_COLLECTION    _("Passed in list is not a list of parse errors.\n")
#define FAILED_TO_PROCCESS  _("Internal Error. Failed to process error list.\n")
#define ERROR_HEADER        _("Parsing errors and warnings in file: %s\n")
/* For grammar errors */
#define WRONG_GRAMMAR       _("Passed in list is not a list of grammar errors.\n")
#define FAILED_TO_PROC_G    _("Internal Error. Failed to process list of grammar errors.\n")
#define ERROR_HEADER_G      _("Logical errors and warnings in file: %s\n")
/* For validation errors */
#define WRONG_VALIDATION    _("Passed in list is not a list of validation errors.\n")
#define FAILED_TO_PROC_V    _("Internal Error. Failed to process list of validation errors.\n")
#define ERROR_HEADER_V      _("Validation errors and warnings in file: %s\n")

#define LINE_FORMAT         _("%s (%d) on line %d: %s")
#define MAX_ERROR_LINE      120

/* Codes that parsing function can return */
#define RET_PAIR        0
#define RET_COMMENT     1
#define RET_SECTION     2
#define RET_INVALID     3
#define RET_EMPTY       4
#define RET_EOF         5
#define RET_ERROR       6

#define INI_ERROR       "errors"
#define INI_METADATA    "meta"
#define INI_ERROR_NAME  "errname"
#define INI_CONFIG_NAME "INI"

#define INI_SECTION_KEY "["

/* Internal sizes. MAX_KEY is defined in config.h */
#define MAX_VALUE       (PATH_MAX + 4096)
#define BUFFER_SIZE     MAX_KEY + MAX_VALUE + 3

/* Beffer length used for int to string conversions */
#define CONVERSION_BUFFER 80

/* Size of the block for a value */
#define INI_VALUE_BLOCK   100

/* Default boundary */
#define INI_WRAP_BOUNDARY 80

/* This constant belongs here. */
#define COL_CLASS_INI_BASE        20000
#define COL_CLASS_INI_CONFIG      COL_CLASS_INI_BASE + 0

/**
 * @brief A one level collection of parse errors.
 *
 * Collection stores \ref parse_error structures.
 */
#define COL_CLASS_INI_PERROR      COL_CLASS_INI_BASE + 2

/**
 * @brief Collection of metadata.
 *
 * Collection that stores metadata.
 */
#define COL_CLASS_INI_META        COL_CLASS_INI_BASE + 4

/* Family of errors */
#define INI_FAMILY_PARSING      0
#define INI_FAMILY_VALIDATION   1
#define INI_FAMILY_GRAMMAR      2

#define INI_MV1S_MASK      0x000F /* Merge values options mask
                                   * for one section */
#define INI_MV2S_MASK      0x00F0 /* Merge values options mask
                                   * for two sections. */
#define INI_MS_MASK        0x0F00 /* Merge section options mask */


/* Different error string functions can be passed as callbacks */
typedef const char * (*error_fn)(int error);

#endif
