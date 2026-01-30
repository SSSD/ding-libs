/*
    INI LIBRARY

    Value interpretation functions for arrays of values
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

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include "trace.h"
#include "collection.h"
#include "collection_tools.h"
#include "ini_defines.h"
#include "ini_configobj.h"
#include "ini_valueobj.h"

/*
 * Internal contants to indicate how
 * to process the lists of strings.
 */
#define EXCLUDE_EMPTY   0
#define INCLUDE_EMPTY   1

/* Maximum number of separators supported. Do not make it less than 3. */
#define MAX_SEP_LEN     3

/* Arrays of stings */
static char **get_str_cfg_array(struct value_obj *vo,
                                int include,
                                const char *sep,
                                int *size,
                                int *error)
{
    char *copy = NULL;
    char *dest = NULL;
    char locsep[MAX_SEP_LEN + 1];
    uint32_t lensep;
    const char *buff;
    uint32_t count = 0;
    uint32_t len = 0;
    uint32_t resume_len;
    char **array;
    const char *start;
    char *start_array;
    uint32_t i, j;
    uint32_t dlen;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Get value and length - no error checking as we checked it above
     * and there is no other reson the function could to fail.
     */
    value_get_concatenated(vo, &buff);
    value_get_concatenated_len(vo, &dlen);

    /* Handle the separators */
    if (sep == NULL) {
        locsep[0] = ',';
        locsep[1] = '\0';
        lensep = 2;
    }
    else {
        strncpy(locsep, sep, MAX_SEP_LEN);
        locsep[MAX_SEP_LEN] = '\0';
        lensep = strlen(locsep) + 1;
    }

    /* Allocate memory for the copy of the string */
    TRACE_INFO_NUMBER("Length to allocate is :", dlen);
    /* Always reserve one more byte
     * for the case when the string consist of delimeters */
    copy = malloc(dlen + 1);
    if (copy == NULL) {
        TRACE_ERROR_NUMBER("Failed to allocate memory.", ENOMEM);
        if (error) *error = ENOMEM;
        return NULL;
    }

    /* Suppress warning */
    start = buff;

    /* Loop through the string */
    dest = copy;
    for(i = 0; i < dlen; i++) {
        for(j = 0; j < lensep; j++) {
            if(buff[i] == locsep[j]) {
                /* If we found one of the separators trim spaces around */
                resume_len = len;
                while (len > 0) {
                    if (isspace(start[len - 1])) len--;
                    else break;
                }
                TRACE_INFO_STRING("Current:", start);
                TRACE_INFO_NUMBER("Length:", len);
                if (len > 0) {
                    /* Save block aside */
                    memcpy(dest, start, len);
                    count++;
                    dest += len;
                    *dest = '\0';
                    dest++;
                }
                else if(include) {
                    count++;
                    *dest = '\0';
                    dest++;
                }

                /* Move forward and trim spaces if any */
                start += resume_len + 1;
                i++;
                TRACE_INFO_STRING("Other pointer :", buff + i);
                while ((i < dlen) && (isspace(*start))) {
                    i++;
                    start++;
                }
                len = -1; /* Len will be increased in the loop */
                i--; /* i will be increased so we need to step back */
                TRACE_INFO_STRING("Remaining buffer after triming spaces:",
                                  start);
                break;
            }
        }
        len++;
    }

    /* Save last segment */
    TRACE_INFO_STRING("Current:", start);
    TRACE_INFO_NUMBER("Length:", len);
    if (len > 0) {
        /* Save block aside */
        memcpy(dest, start, len);
        count++;
        dest += len;
        *dest = '\0';
    }
    else if(include && dlen && count) {
        TRACE_INFO_NUMBER("Include :", include);
        TRACE_INFO_NUMBER("dlen :", dlen);
        TRACE_INFO_NUMBER("Count :", count);
        count++;
        *dest = '\0';
    }

    /* Now we know how many items are there in the list */
    array = malloc((count + 1) * sizeof(char *));
    if (array == NULL) {
        free(copy);
        TRACE_ERROR_NUMBER("Failed to allocate memory.", ENOMEM);
        if (error) *error = ENOMEM;
        return NULL;
    }

    /* Loop again to fill in the pointers */
    start_array = copy;
    for (i = 0; i < count; i++) {
        TRACE_INFO_STRING("Token :", start_array);
        TRACE_INFO_NUMBER("Item :", i);
        array[i] = start_array;
        /* Move to next item */
        while(*start_array) start_array++;
        start_array++;
    }
    array[count] = NULL;

    if (error) *error = EOK;
    if (size) *size = count;
    /* If count is 0 the copy needs to be freed */
    if (count == 0) free(copy);

    TRACE_FLOW_EXIT();
    return array;
}

/* Get array of strings from item eliminating empty tokens */
char **ini_get_string_config_array(struct value_obj *vo,
                                   const char *sep, int *size, int *error)
{
    TRACE_FLOW_ENTRY();
    return get_str_cfg_array(vo, EXCLUDE_EMPTY, sep, size, error);
}
/* Get array of strings from item preserving empty tokens */
char **ini_get_raw_string_config_array(struct value_obj *vo,
                                       const char *sep, int *size, int *error)
{
    TRACE_FLOW_ENTRY();
    return get_str_cfg_array(vo, INCLUDE_EMPTY, sep, size, error);
}

/* Special function to free string config array */
void ini_free_string_config_array(char **str_config)
{
    TRACE_FLOW_ENTRY();

    if (str_config != NULL) {
        if (*str_config != NULL) free(*str_config);
        free(str_config);
    }

    TRACE_FLOW_EXIT();
}

/* Get an array of long values.
 * NOTE: For now I leave just one function that returns numeric arrays.
 * In future if we need other numeric types we can change it to do strtoll
 * internally and wrap it for backward compatibility.
 */
long *ini_get_long_config_array(struct value_obj *vo, int *size, int *error)
{
    const char *str;
    char *endptr;
    long val = 0;
    long *array;
    uint32_t count = 0;
    int err;
    uint32_t dlen;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid value object argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Do we have the size ? */
    if (size == NULL) {
        TRACE_ERROR_NUMBER("Invalid size argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Get value and length - no error checking as we checked it above
     * and there is no other reson the function could to fail.
     */
    value_get_concatenated(vo, &str);
    value_get_concatenated_len(vo, &dlen);

    /* Assume that we have maximum number of different numbers */
    array = (long *)malloc(sizeof(long) * dlen/2);
    if (array == NULL) {
        TRACE_ERROR_NUMBER("Failed to allocate memory.", ENOMEM);
        if (error) *error = ENOMEM;
        return NULL;
    }

    /* Now parse the string */
    while (*str) {

        errno = 0;
        val = strtol(str, &endptr, 10);
        err = errno;

        if (err) {
            TRACE_ERROR_NUMBER("Conversion failed", err);
            free(array);
            if (error) *error = err;
            return NULL;
        }

        if (endptr == str) {
            TRACE_ERROR_NUMBER("Nothing processed", EIO);
            free(array);
            if (error) *error = EIO;
            return NULL;
        }

        /* Save value */
        array[count] = val;
        count++;
        /* Are we done? */
        if (*endptr == 0) break;
        /* Advance to the next valid number */
        for (str = endptr; *str; str++) {
            if (isdigit(*str) || (*str == '-') || (*str == '+')) break;
        }
    }

    *size = count;
    if (error) *error = EOK;

    TRACE_FLOW_EXIT();
    return array;

}

/* Get an array of double values */
double *ini_get_double_config_array(struct value_obj *vo, int *size, int *error)
{
    const char *str;
    char *endptr;
    double val = 0;
    double *array;
    int count = 0;
    struct lconv *loc;
    uint32_t dlen;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if (vo == NULL) {
        TRACE_ERROR_NUMBER("Invalid value object argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Do we have the size ? */
    if (size == NULL) {
        TRACE_ERROR_NUMBER("Invalid size argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Get value and length - no error checking as we checked it above
     * and there is no other reson the function could to fail.
     */
    value_get_concatenated(vo, &str);
    value_get_concatenated_len(vo, &dlen);


    /* Assume that we have maximum number of different numbers */
    array = (double *)malloc(sizeof(double) * dlen/2);
    if (array == NULL) {
        TRACE_ERROR_NUMBER("Failed to allocate memory.", ENOMEM);
        if (error) *error = ENOMEM;
        return NULL;
    }

    /* Get locale information so that we can check for decimal point character.
     * Based on the man pages it is unclear if this is an allocated memory or not.
     * Seems like it is a static thread or process local structure so
     * I will not try to free it after use.
     */
    loc = localeconv();

    /* Now parse the string */
    while (*str) {
        TRACE_INFO_STRING("String to convert",str);
        errno = 0;
        val = strtod(str, &endptr);
        if ((errno == ERANGE) ||
            ((errno != 0) && (val == 0)) ||
            (endptr == str)) {
            TRACE_ERROR_NUMBER("Conversion failed", EIO);
            free(array);
            if (error) *error = EIO;
            return NULL;
        }
        /* Save value */
        array[count] = val;
        count++;
        /* Are we done? */
        if (*endptr == 0) break;
        TRACE_INFO_STRING("End pointer after conversion",endptr);
        /* Advance to the next valid number */
        for (str = endptr; *str; str++) {
            if (isdigit(*str) || (*str == '-') || (*str == '+') ||
               /* It is ok to do this since the string is null terminated */
               ((*str == *(loc->decimal_point)) && isdigit(str[1]))) break;
        }
    }

    *size = count;
    if (error) *error = EOK;

    TRACE_FLOW_EXIT();
    return array;

}

/* Special function to free long config array */
void ini_free_long_config_array(long *array)
{
    free(array);
}

/* Special function to free double config array */
void ini_free_double_config_array(double *array)
{
    free(array);
}
