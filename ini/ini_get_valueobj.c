/*
    INI LIBRARY

    Value interpretation functions for single values
    and corresponding memory cleanup functions.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2012

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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "trace.h"
#include "collection.h"
#include "collection_tools.h"
#include "ini_defines.h"
#include "ini_config_priv.h"
#include "ini_configobj.h"
#include "ini_valueobj.h"

/* Macro co convert to HEX value */
#define HEXVAL(c) (isdigit(c) ? (c - '0') : (tolower(c) - 'a') + 10)

static int is_same_section(struct ini_cfgobj *ini_config,
                           const char *section)
{
    int len = 0;

    TRACE_FLOW_ENTRY();

    /* If section is not defined it is different */
    if (ini_config->section == NULL) {
        TRACE_FLOW_RETURN(0);
        return 0;
    }

    len = strlen(section);

    /* If values are same this is the same section */
    if ((strncasecmp(ini_config->section, section, ini_config->section_len) == 0) &&
        (ini_config->section_len == len)) {
        TRACE_FLOW_RETURN(1);
        return 1;
    }

    /* Otherwise the values are different */
    TRACE_FLOW_RETURN(0);
    return 0;
}

static int is_same_name(struct ini_cfgobj *ini_config,
                        const char *name,
                        int name_len)
{
    TRACE_FLOW_ENTRY();

    /* If name is not defined it is different */
    if (ini_config->name == NULL) {
        TRACE_FLOW_RETURN(0);
        return 0;
    }

    /* If values are same this is the same value */
    if ((strncasecmp(ini_config->name, name, ini_config->name_len) == 0) &&
        (ini_config->name_len == name_len)) {
        TRACE_FLOW_RETURN(1);
        return 1;
    }

    /* Otherwise the values are different */
    TRACE_FLOW_RETURN(0);
    return 0;
}


/* Function to get value object from the configuration handle */
int ini_get_config_valueobj(const char *section,
                            const char *name,
                            struct ini_cfgobj *ini_config,
                            int mode,
                            struct value_obj **vo)
{
    int error = EOK;
    struct collection_item *section_handle = NULL;
    struct collection_item *item = NULL;
    const char *to_find;
    char default_section[] = INI_DEFAULT_SECTION;
    uint64_t hash = 0;
    int len = 0, name_len = 0;

    TRACE_FLOW_ENTRY();

    /* Do we have the accepting memory ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument vo.", EINVAL);
        return EINVAL;
    }

    *vo = NULL;

    if (ini_config == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument ini_config.", EINVAL);
        return EINVAL;
    }

    if ((mode < INI_GET_FIRST_VALUE) ||
        (mode > INI_GET_NEXT_VALUE)) {
        TRACE_ERROR_NUMBER("Invalid argument mode:", mode);
        return EINVAL;
    }

    /* Do we have a name ? */
    if (name == NULL) {
        TRACE_ERROR_NUMBER("Name is NULL it will not be found.", EINVAL);
        return EINVAL;
    }

    /* Empty section means look for the default one */
    if (section == NULL) to_find = default_section;
    else to_find = section;

    TRACE_INFO_STRING("Getting Name:", name);
    TRACE_INFO_STRING("In Section:", to_find);

    /* Make sure we start over if this is the first value */
    if (mode == INI_GET_FIRST_VALUE) ini_config_clean_state(ini_config);

    /* Are we looking in the same section ? */
    if (!is_same_section(ini_config, to_find)) {

        /* This is a different section */
        ini_config_clean_state(ini_config);

        /* Get Subcollection */
        error = col_get_collection_reference(ini_config->cfg, &section_handle, to_find);
        /* Check error */
        if (error && (error != ENOENT)) {
            TRACE_ERROR_NUMBER("Failed to get section", error);
            return error;
        }

        /* Did we find a section */
        if ((error == ENOENT) || (section_handle == NULL)) {
            /* We have not found section - return success */
                TRACE_FLOW_EXIT();
            return EOK;
        }

        /* Create an iterator */
        error = col_bind_iterator(&(ini_config->iterator),
                                  section_handle,
                                  COL_TRAVERSE_ONELEVEL);
        /* Make sure we free the section we found */
        col_destroy_collection(section_handle);
        /* Check error */
        if (error) {
            TRACE_ERROR_NUMBER("Failed to bind to section", error);
            return error;
        }

        /* Save section */
        ini_config->section_len = strlen(to_find);
        ini_config->section = strndup(to_find, ini_config->section_len);
        /* Check error */
        if (ini_config->section == NULL) {
            TRACE_ERROR_NUMBER("Failed to save section name ", ENOMEM);
            ini_config_clean_state(ini_config);
            return ENOMEM;
        }
    }

    hash = col_make_hash(name, 0, &name_len);

    /* Check if this is the same name */
    if (!is_same_name(ini_config, name, name_len)) {
        TRACE_INFO_STRING("Saved name:", ini_config->name);
        TRACE_INFO_STRING("Passed name:", name);
        TRACE_INFO_NUMBER("Length of the saved name", ini_config->name_len);
        TRACE_INFO_NUMBER("Length of the passed name", name_len);
        col_rewind_iterator(ini_config->iterator);
        free(ini_config->name);
        ini_config->name = NULL;
        ini_config->name_len = 0;
    }

    /* Iterate through the section */
    do {

        /* Loop through a collection */
        error = col_iterate_collection(ini_config->iterator, &item);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to iterate", error);
            ini_config_clean_state(ini_config);
            return error;
        }

        /* Are we done ? */
        if (item == NULL) {
            /* There is nothing left to look for */
            ini_config_clean_state(ini_config);
            TRACE_FLOW_EXIT();
            return EOK;
        }

        if ((hash == (unsigned long int)col_get_item_hash(item)) &&
            (strncasecmp(col_get_item_property(item, &len), name, name_len) == 0) &&
            (len == name_len)) {
                TRACE_INFO_STRING("Item is found", name);
                break;
        }
    }
    while(1);

    if (!is_same_name(ini_config, name, name_len)) {
        /* Save name */
        ini_config->name_len = name_len;
        ini_config->name = strndup(name, name_len);
        /* Check error */
        if (ini_config->name == NULL) {
            TRACE_ERROR_NUMBER("Failed to save key name ", ENOMEM);
            ini_config_clean_state(ini_config);
            return ENOMEM;
        }
    }

    *vo = *((struct value_obj **)(col_get_item_data(item)));

    TRACE_FLOW_EXIT();
    return error;
}

/* Get long long value from config value object */
static long long ini_get_llong_config_value(struct value_obj *vo,
                                            int strict,
                                            long long def,
                                            int *error)
{
    int err;
    const char *str;
    char *endptr;
    long long val = 0;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return def;
    }

    if (error) *error = EOK;

    /* Get value - no error checking as we checked it above
     * and there is no other reson the function could fail.
     */
    value_get_concatenated(vo, &str);

    /* Try to parse the value */
    errno = 0;
    val = strtoll(str, &endptr, 10);
    err = errno;

    /* Check for various possible errors */
    if (err != 0) {
        TRACE_ERROR_NUMBER("Conversion failed", err);
        if (error) *error = err;
        return def;
    }

    /* Other error cases */
    if ((endptr == str) || (strict && (*endptr != '\0'))) {
        TRACE_ERROR_NUMBER("More characters or nothing processed", EIO);
        if (error) *error = EIO;
        return def;
    }

    TRACE_FLOW_NUMBER("ini_get_llong_config_value returning", (long)val);
    return val;
}

/* Get unsigned long long value from config value object */
static unsigned long long ini_get_ullong_config_value(struct value_obj *vo,
                                                      int strict,
                                                      unsigned long long def,
                                                      int *error)
{
    int err;
    const char *str;
    char *endptr;
    unsigned long long val = 0;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return def;
    }

    if (error) *error = EOK;

    /* Get value - no error checking as we checked it above
     * and there is no other reson the function could fail.
     */
    value_get_concatenated(vo, &str);

    errno = 0;
    val = strtoull(str, &endptr, 10);
    err = errno;

    /* Check for various possible errors */
    if (err != 0) {
        TRACE_ERROR_NUMBER("Conversion failed", err);
        if (error) *error = err;
        return def;
    }

    /* Other error cases */
    if ((endptr == str) || (strict && (*endptr != '\0'))) {
        TRACE_ERROR_NUMBER("More characters or nothing processed", EIO);
        if (error) *error = EIO;
        return def;
    }

    TRACE_FLOW_NUMBER("ini_get_ullong_config_value returning", val);
    return val;
}


/* Get integer value from config value */
int ini_get_int_config_value(struct value_obj *vo,
                             int strict,
                             int def,
                             int *error)
{
    long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_llong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if ((val > INT_MAX) || (val < INT_MIN)) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_int_config_value returning", (int)val);
    return (int)val;
}

/* Get unsigned integer value from config value object */
unsigned ini_get_unsigned_config_value(struct value_obj *vo,
                                       int strict,
                                       unsigned def,
                                       int *error)
{
    unsigned long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_ullong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if (val > UINT_MAX) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_unsigned_config_value returning",
                      (unsigned)val);
    return (unsigned)val;
}

/* Get long value from config value object */
long ini_get_long_config_value(struct value_obj *vo,
                               int strict,
                               long def,
                               int *error)
{
    long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_llong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if ((val > LONG_MAX) || (val < LONG_MIN)) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_long_config_value returning",
                      (long)val);
    return (long)val;
}

/* Get unsigned long value from config value object */
unsigned long ini_get_ulong_config_value(struct value_obj *vo,
                                         int strict,
                                         unsigned long def,
                                         int *error)
{
    unsigned long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_ullong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if (val > ULONG_MAX) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_ulong_config_value returning",
                      (unsigned long)val);
    return (unsigned long)val;
}

/* Get int32_t value from config value object */
int32_t ini_get_int32_config_value(struct value_obj *vo,
                                   int strict,
                                   int32_t def,
                                   int *error)
{
    long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_llong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if ((val > INT32_MAX) || (val < INT32_MIN)) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_int32_config_value returning",
                      (int32_t)val);
    return (int32_t)val;
}

/* Get uint32_t value from config value object */
uint32_t ini_get_uint32_config_value(struct value_obj *vo,
                                     int strict,
                                     uint32_t def,
                                     int *error)
{
    unsigned long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_ullong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if (val > UINT32_MAX) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_uint32_config_value returning",
                      (uint32_t)val);
    return (uint32_t)val;
}

/* Get int64_t value from config value ovject */
int64_t ini_get_int64_config_value(struct value_obj *vo,
                                   int strict,
                                   int64_t def,
                                   int *error)
{
    long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_llong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if ((val > INT64_MAX) || (val < INT64_MIN)) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_int64_config_value returning",
                      (int64_t)val);
    return (int64_t)val;
}

/* Get uint64_t value from config value object */
uint64_t ini_get_uint64_config_value(struct value_obj *vo,
                                     int strict,
                                     uint64_t def,
                                     int *error)
{
    unsigned long long val = 0;
    int err = 0;

    TRACE_FLOW_ENTRY();

    val = ini_get_ullong_config_value(vo, strict, def, &err);
    if (err == 0) {
        if (val > UINT64_MAX) {
            TRACE_ERROR_NUMBER("Value is out of range", ERANGE);
            val = def;
            err = ERANGE;
        }
    }

    if (error) *error = err;

    TRACE_FLOW_NUMBER("ini_get_uint64_config_value returning",
                      (uint64_t)val);
    return (uint64_t)val;
}

/* Get double value */
double ini_get_double_config_value(struct value_obj *vo,
                                   int strict, double def, int *error)
{
    const char *str;
    char *endptr;
    double val = 0;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return def;
    }

    if (error) *error = EOK;

    /* Get value - no error checking as we checked it above
     * and there is no other reason the function could fail.
     */
    value_get_concatenated(vo, &str);

    errno = 0;
    val = strtod(str, &endptr);

    /* Check for various possible errors */
    if ((errno == ERANGE) ||
        ((errno != 0) && (val == 0)) ||
        (endptr == str)) {
        TRACE_ERROR_NUMBER("Conversion failed", EIO);
        if (error) *error = EIO;
        return def;
    }

    if (strict && (*endptr != '\0')) {
        TRACE_ERROR_NUMBER("More characters than expected", EIO);
        if (error) *error = EIO;
        val = def;
    }

    TRACE_FLOW_DOUBLE("ini_get_double_config_value returning", val);
    return val;
}

/* Get boolean value */
unsigned char ini_get_bool_config_value(struct value_obj *vo,
                                        unsigned char def, int *error)
{
    const char *str;
    uint32_t len = 0;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return def;
    }

    if (error) *error = EOK;

    /* Get value - no error checking as we checked it above
     * and there is no other reson the function could fail.
     */
    value_get_concatenated(vo, &str);
    value_get_concatenated_len(vo, &len);

    /* Try to parse the value */
    if ((strncasecmp(str, "true", len) == 0) ||
        (strncasecmp(str, "yes", len) == 0)) {
        TRACE_FLOW_STRING("Returning", "true");
        return '\1';
    }
    else if ((strncasecmp(str, "false", len) == 0) ||
             (strncasecmp(str, "no", len) == 0)) {
        TRACE_FLOW_STRING("Returning", "false");
        return '\0';
    }

    TRACE_ERROR_STRING("Returning", "error");
    if (error) *error = EIO;
    return def;
}

/* Return a string out of the value */
char *ini_get_string_config_value(struct value_obj *vo,
                              int *error)
{
    const char *str = NULL;
    char *ret_str = NULL;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Get value - no error checking as we checked it above
     * and there is no other reson the function could fail.
     */
    value_get_concatenated(vo, &str);

    ret_str = strdup(str);
    if (ret_str == NULL) {
        TRACE_ERROR_NUMBER("Failed to allocate memory.", ENOMEM);
        if (error) *error = ENOMEM;
        return NULL;
    }

    if (error) *error = EOK;

    TRACE_FLOW_STRING("ini_get_string_config_value returning", str);
    return ret_str;
}

/* Get string from the value object */
const char *ini_get_const_string_config_value(struct value_obj *vo,
                                              int *error)
{
    const char *str;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Get value - no error checking as we checked it above
     * and there is no other reson the function could fail.
     */
    value_get_concatenated(vo, &str);

    if (error) *error = EOK;

    TRACE_FLOW_STRING("ini_get_const_string_config_value returning", str);
    return str;
}

/* A special hex format is assumed.
 * The string should be taken in single quotes
 * and consist of hex encoded value two hex digits per byte.
 * Example: '0A2BFECC'
 * Case does not matter.
 */
char *ini_get_bin_config_value(struct value_obj *vo,
                               int *length, int *error)
{
    int i;
    char *value = NULL;
    const char *buff;
    int size = 0;
    uint32_t len = 0;
    const char *str;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    if (error) *error = EOK;

    /* Get value - no error checking as we checked it above
     * and there is no other reson the function could fail.
     */
    value_get_concatenated(vo, &str);

    /* Check the length */
    value_get_concatenated_len(vo, &len);
    if ((len%2) != 0) {
        TRACE_ERROR_STRING("Invalid length for binary data", "");
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Is the format correct ? */
    if ((*str != '\'') ||
        (str[len -1] != '\'')) {
        TRACE_ERROR_STRING("String is not escaped","");
        if (error) *error = EIO;
        return NULL;
    }

    /* Check that all the symbols are ok */
    buff = str + 1;
    len -= 2;
    for (i = 0; i < len; i++) {
        if (!isxdigit(buff[i])) {
            TRACE_ERROR_STRING("Invalid encoding for binary data", buff);
            if (error) *error = EIO;
            return NULL;
        }
    }

    /* The value is good so we can allocate memory for it */
    value = malloc(len / 2);
    if (value == NULL) {
        TRACE_ERROR_NUMBER("Failed to allocate memory.", ENOMEM);
        if (error) *error = ENOMEM;
        return NULL;
    }

    /* Convert the value */
    for (i = 0; i < len; i += 2) {
        value[size] = (char)(16 * HEXVAL(buff[i]) + HEXVAL(buff[i+1]));
        size++;
    }

    if (error) *error = EOK;
    if (length) *length = size;
    TRACE_FLOW_STRING("ini_get_bin_config_value", "Exit");
    return value;
}

/* Function to free binary configuration value */
void ini_free_bin_config_value(char *value)
{
    free(value);
}
