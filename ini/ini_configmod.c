/*
    INI LIBRARY

    Implementation of the modification interface.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2014

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

#define _GNU_SOURCE /* for asprintf */
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "trace.h"
#include "ref_array.h"
#include "simplebuffer.h"
#include "collection.h"
#include "ini_comment.h"
#include "ini_defines.h"
#include "ini_valueobj.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "ini_configmod.h"

/* Which kind of search we should use? */
#define EXACT(a) ((a == INI_VA_MOD_E) || (a == INI_VA_MODADD_E)) ? 1 : 0

static void cb(const char *property,
               int property_len,
               int type,
               void *data,
               int length,
               void *ext_data);


/* Delete value by key or position */
int ini_config_delete_value(struct ini_cfgobj *ini_config,
                            const char *section,
                            int position,
                            const char *key,
                            int idx)
{
    int error = EOK;
    struct value_obj *vo = NULL;
    struct collection_item *item = NULL;

    TRACE_FLOW_ENTRY();

    /* Check arguments */
    if (!ini_config) {
        TRACE_ERROR_STRING("Invalid argument","ini_config");
        return EINVAL;
    }

    if (!section) {
        TRACE_ERROR_STRING("Invalid argument","section");
        return EINVAL;
    }

    if (!key) {
        TRACE_ERROR_STRING("Invalid argument","key");
        return EINVAL;
    }

    if (idx < 0) {
        TRACE_ERROR_STRING("Invalid argument","idx");
        return EINVAL;
    }

    error = col_extract_item(ini_config->cfg,
                             section,
                             position,
                             key,
                             idx,
                             COL_TYPE_ANY,
                             &item);
    if (error) {
        TRACE_ERROR_NUMBER("Item not found or error",
                            error);
        return error;
    }

    vo = *((struct value_obj **)(col_get_item_data(item)));
    value_destroy(vo);

    col_delete_item(item);

    TRACE_FLOW_EXIT();
    return error;
}


/* Updates a comment for value that is found by seaching for a specific key */
int ini_config_update_comment(struct ini_cfgobj *ini_config,
                              const char *section,
                              const char *key,
                              const char *comments[],
                              size_t count_comment,
                              int idx)
{
    int error = EOK;
    struct ini_comment *ic = NULL;
    struct value_obj *vo = NULL;
    struct collection_item *item = NULL;

    TRACE_FLOW_ENTRY();

    /* Check arguments */
    if (!ini_config) {
        TRACE_ERROR_STRING("Invalid argument","ini_config");
        return EINVAL;
    }

    if (!section) {
        TRACE_ERROR_STRING("Invalid argument","section");
        return EINVAL;
    }

    if (!key) {
        TRACE_ERROR_STRING("Invalid argument","key");
        return EINVAL;
    }

    if (idx < 0) {
        TRACE_ERROR_STRING("Invalid argument","idx");
        return EINVAL;
    }

    /* Look for the exact item */
    error = col_get_dup_item(ini_config->cfg,
                             section,
                             key,
                             COL_TYPE_ANY,
                             idx,
                             1,
                             &item);

    if (error) {
        TRACE_ERROR_NUMBER("Item not found or error",
                            error);
        return error;
    }

    /* If item not found return error */
    if (!item) {
        TRACE_ERROR_NUMBER("Item not found.", ENOENT);
        return ENOENT;
    }

    /* Build comment */
    if (comments) {
        error = ini_comment_construct(comments,
                                      count_comment,
                                      &ic);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to construct comment", error);
            return error;
        }
    }

    vo = *((struct value_obj **)(col_get_item_data(item)));

    /* Replace comment with the new one. Old one is freed by the function */
    error = value_put_comment(vo, ic);
    if (error) {
        TRACE_ERROR_NUMBER("Faile to update comment.",
                            error);
        ini_comment_destroy(ic);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Add or modify a value that stores array of integers */
int ini_config_add_int_arr_value(struct ini_cfgobj *ini_config,
                                 const char *section,
                                 const char *key,
                                 int *value_int_arr,
                                 size_t count_int,
                                 char sep,
                                 const char *comments[],
                                 size_t count_comment,
                                 int border,
                                 int position,
                                 const char *other_key,
                                 int idx,
                                 enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char **str_arr = NULL;
    size_t i = 0;
    size_t j = 0;

    TRACE_FLOW_ENTRY();

    if (!count_int) {
        TRACE_ERROR_STRING("Invalid argument","count_int");
        return EINVAL;
    }

    str_arr = calloc(count_int, sizeof(char *));
    if (!str_arr) {
        TRACE_ERROR_STRING("Failed to allocate memory for string array",
                           ENOMEM);
        return ENOMEM;
    }

    for (i = 0; i < count_int; i++) {
        ret = asprintf(&str_arr[i], "%d", value_int_arr[i]);
        if (ret == -1) {
            TRACE_ERROR_NUMBER("Asprintf failed.", ret);
            /* The main reason is propbaly memory allocation */
            for (j = 0; j < i; j++) free(str_arr[j]);
            free(str_arr);
            return ENOMEM;
        }
    }

    error = ini_config_add_str_arr_value(ini_config,
                                         section,
                                         key,
                                         str_arr,
                                         count_int,
                                         sep,
                                         comments,
                                         count_comment,
                                         border,
                                         position,
                                         other_key,
                                         idx,
                                         flags);

    for (i = 0; i < count_int; i++) free(str_arr[i]);
    free(str_arr);

    TRACE_FLOW_RETURN(error);
    return error;
}

/* Add or modify a value that stores array of long ints */
int ini_config_add_long_arr_value(struct ini_cfgobj *ini_config,
                                  const char *section,
                                  const char *key,
                                  long *value_long_arr,
                                  size_t count_long,
                                  char sep,
                                  const char *comments[],
                                  size_t count_comment,
                                  int border,
                                  int position,
                                  const char *other_key,
                                  int idx,
                                  enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char **str_arr = NULL;
    size_t i = 0;
    size_t j = 0;

    TRACE_FLOW_ENTRY();

    if (!count_long) {
        TRACE_ERROR_STRING("Invalid argument","count_long");
        return EINVAL;
    }

    str_arr = calloc(count_long, sizeof(char *));
    if (!str_arr) {
        TRACE_ERROR_STRING("Failed to allocate memory for string array",
                           ENOMEM);
        return ENOMEM;
    }

    for (i = 0; i < count_long; i++) {
        ret = asprintf(&str_arr[i], "%ld", value_long_arr[i]);
        if (ret == -1) {
            TRACE_ERROR_NUMBER("Asprintf failed.", ret);
            /* The main reason is propbaly memory allocation */
            for (j = 0; j < i; j++) free(str_arr[j]);
            free(str_arr);
            return ENOMEM;
        }
    }

    error = ini_config_add_str_arr_value(ini_config,
                                         section,
                                         key,
                                         str_arr,
                                         count_long,
                                         sep,
                                         comments,
                                         count_comment,
                                         border,
                                         position,
                                         other_key,
                                         idx,
                                         flags);

    for (i = 0; i < count_long; i++) free(str_arr[i]);
    free(str_arr);

    TRACE_FLOW_RETURN(error);
    return error;
}

/* Add or modify a value that stores array of doubles */
int ini_config_add_double_arr_value(struct ini_cfgobj *ini_config,
                                    const char *section,
                                    const char *key,
                                    double *value_double_arr,
                                    size_t count_double,
                                    char sep,
                                    const char *comments[],
                                    size_t count_comment,
                                    int border,
                                    int position,
                                    const char *other_key,
                                    int idx,
                                    enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char **str_arr = NULL;
    size_t i = 0;
    size_t j = 0;

    TRACE_FLOW_ENTRY();

    if (!count_double) {
        TRACE_ERROR_STRING("Invalid argument","count_double");
        return EINVAL;
    }

    str_arr = calloc(count_double, sizeof(char *));
    if (!str_arr) {
        TRACE_ERROR_STRING("Failed to allocate memory for string array",
                           ENOMEM);
        return ENOMEM;
    }

    for (i = 0; i < count_double; i++) {
        ret = asprintf(&str_arr[i], "%f", value_double_arr[i]);
        if (ret == -1) {
            TRACE_ERROR_NUMBER("Asprintf failed.", ret);
            /* The main reason is propbaly memory allocation */
            for (j = 0; j < i; j++) free(str_arr[j]);
            free(str_arr);
            return ENOMEM;
        }
    }

    error = ini_config_add_str_arr_value(ini_config,
                                         section,
                                         key,
                                         str_arr,
                                         count_double,
                                         sep,
                                         comments,
                                         count_comment,
                                         border,
                                         position,
                                         other_key,
                                         idx,
                                         flags);

    for (i = 0; i < count_double; i++) free(str_arr[i]);
    free(str_arr);

    TRACE_FLOW_RETURN(error);
    return error;
}

/* Add or modify a value that stores array of strings */
int ini_config_add_const_str_arr_value(struct ini_cfgobj *ini_config,
                                       const char *section,
                                       const char *key,
                                       const char *value_str_arr[],
                                       size_t count_str,
                                       char sep,
                                       const char *comments[],
                                       size_t count_comment,
                                       int border,
                                       int position,
                                       const char *other_key,
                                       int idx,
                                       enum INI_VA flags)
{
    int error = EOK;
    size_t len = 0;
    size_t i = 0;
    struct simplebuffer *sbobj = NULL;
    char sp[3] = "  ";

    TRACE_FLOW_ENTRY();

    if (!count_str) {
        TRACE_ERROR_STRING("Invalid argument","count_str");
        return EINVAL;
    }

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    sp[0] = sep;

    for (i = 0; i < count_str - 1; i++) {
        len = strlen(value_str_arr[i]);
        if ((error = simplebuffer_add_str(sbobj,
                                          value_str_arr[i],
                                          len,
                                          INI_VALUE_BLOCK)) ||
            (error = simplebuffer_add_str(sbobj,
                                          sp,
                                          2,
                                          INI_VALUE_BLOCK))) {
            TRACE_ERROR_NUMBER("String append failed.", error);
            simplebuffer_free(sbobj);
            return error;
        }
    }
    len = strlen(value_str_arr[count_str - 1]);
    error = simplebuffer_add_str(sbobj,
                                 value_str_arr[count_str - 1],
                                 len,
                                 INI_VALUE_BLOCK);
    if (error) {
        TRACE_ERROR_NUMBER("String append failed.", error);
        simplebuffer_free(sbobj);
        return error;
    }

    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     (const char *)simplebuffer_get_buf(sbobj),
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);

    simplebuffer_free(sbobj);

    TRACE_FLOW_RETURN(error);
    return error;
}

/* Add or modify a value that stores array of strings */
int ini_config_add_str_arr_value(struct ini_cfgobj *ini_config,
                                 const char *section,
                                 const char *key,
                                 char *value_str_arr[],
                                 size_t count_str,
                                 char sep,
                                 const char *comments[],
                                 size_t count_comment,
                                 int border,
                                 int position,
                                 const char *other_key,
                                 int idx,
                                 enum INI_VA flags)
{
    const char **const_str_arr = (const char **)(intptr_t)value_str_arr;

    return ini_config_add_const_str_arr_value(ini_config,
                                              section,
                                              key,
                                              const_str_arr,
                                              count_str,
                                              sep,
                                              comments,
                                              count_comment,
                                              border,
                                              position,
                                              other_key,
                                              idx,
                                              flags);
}

/* Function to add integer value */
int ini_config_add_int_value(struct ini_cfgobj *ini_config,
                             const char *section,
                             const char *key,
                             int value,
                             const char **comments,
                             size_t count_comment,
                             int border,
                             int position,
                             const char *other_key,
                             int idx,
                             enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%d", value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}


/* Function to add long value */
int ini_config_add_long_value(struct ini_cfgobj *ini_config,
                              const char *section,
                              const char *key,
                              long value,
                              const char **comments,
                              size_t count_comment,
                              int border,
                              int position,
                              const char *other_key,
                              int idx,
                              enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%ld", value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}


/* Function to add ulong value */
int ini_config_add_ulong_value(struct ini_cfgobj *ini_config,
                               const char *section,
                               const char *key,
                               unsigned long value,
                               const char **comments,
                               size_t count_comment,
                               int border,
                               int position,
                               const char *other_key,
                               int idx,
                               enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%lu", value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}


/* Function to add unsigned value */
int ini_config_add_unsigned_value(struct ini_cfgobj *ini_config,
                                  const char *section,
                                  const char *key,
                                  unsigned value,
                                  const char **comments,
                                  size_t count_comment,
                                  int border,
                                  int position,
                                  const char *other_key,
                                  int idx,
                                  enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%u", value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}

/* Function to add int32 value */
int ini_config_add_int32_value(struct ini_cfgobj *ini_config,
                               const char *section,
                               const char *key,
                               int32_t value,
                               const char **comments,
                               size_t count_comment,
                               int border,
                               int position,
                               const char *other_key,
                               int idx,
                               enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%"PRId32, value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}

/* Function to add uint32 value */
int ini_config_add_uint32_value(struct ini_cfgobj *ini_config,
                                const char *section,
                                const char *key,
                                uint32_t value,
                                const char **comments,
                                size_t count_comment,
                                int border,
                                int position,
                                const char *other_key,
                                int idx,
                                enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%"PRIu32, value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}

/* Function to add int64 value */
int ini_config_add_int64_value(struct ini_cfgobj *ini_config,
                               const char *section,
                               const char *key,
                               int64_t value,
                               const char **comments,
                               size_t count_comment,
                               int border,
                               int position,
                               const char *other_key,
                               int idx,
                               enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%"PRId64, value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}

/* Function to add uint64 value */
int ini_config_add_uint64_value(struct ini_cfgobj *ini_config,
                                  const char *section,
                                  const char *key,
                                  uint64_t value,
                                  const char **comments,
                                  size_t count_comment,
                                  int border,
                                  int position,
                                  const char *other_key,
                                  int idx,
                                  enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%"PRIu64, value);
    if (ret == -1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}


/* Function to add double value */
int ini_config_add_double_value(struct ini_cfgobj *ini_config,
                                const char *section,
                                const char *key,
                                double value,
                                const char **comments,
                                size_t count_comment,
                                int border,
                                int position,
                                const char *other_key,
                                int idx,
                                enum INI_VA flags)
{
    int error = EOK;
    int ret = 0;
    char *strval = NULL;

    TRACE_FLOW_ENTRY();

    ret = asprintf(&strval, "%f", value);
    if (ret == 1) {
        TRACE_ERROR_NUMBER("Asprintf failed.", ret);
        /* The main reason is propbaly memory allocation */
        return ENOMEM;
    }

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    TRACE_FLOW_RETURN(error);
    free(strval);
    return error;
}

/* Function to add binary value */
int ini_config_add_bin_value(struct ini_cfgobj *ini_config,
                             const char *section,
                             const char *key,
                             void *value,
                             size_t value_len,
                             const char **comments,
                             size_t count_comment,
                             int border,
                             int position,
                             const char *other_key,
                             int idx,
                             enum INI_VA flags)
{
    int error = EOK;
    size_t i;
    char *strval = NULL;
    char *ptr = NULL;

    TRACE_FLOW_ENTRY();

    /* Do we have the vo ? */
    if ((!value) && (value_len)) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        return EINVAL;
    }

    /* The value is good so we can allocate memory for it */
    strval = malloc(value_len * 2 + 3);
    if (strval == NULL) {
        TRACE_ERROR_NUMBER("Failed to allocate memory.", ENOMEM);
        return ENOMEM;
    }

    strval[0] = '\'';

    /* Convert the value */
    ptr = strval + 1;
    for (i = 0; i < value_len; i++) {
        sprintf(ptr, "%02x", *((unsigned char *)(value) + i));
        ptr += 2;
    }

    strval[value_len * 2 + 1] = '\'';
    strval[value_len * 2 + 2] = '\0';

    /* Call string function */
    error = ini_config_add_str_value(ini_config,
                                     section,
                                     key,
                                     strval,
                                     comments,
                                     count_comment,
                                     border,
                                     position,
                                     other_key,
                                     idx,
                                     flags);
    free(strval);
    TRACE_FLOW_RETURN(error);
    return error;
}


/* Function to add string value */
int ini_config_add_str_value(struct ini_cfgobj *ini_config,
                             const char *section,
                             const char *key,
                             const char *value,
                             const char **comments,
                             size_t count_comment,
                             int border,
                             int position,
                             const char *other_key,
                             int idx,
                             enum INI_VA flags)
{
    int error = EOK;
    struct ini_comment *ic = NULL;
    struct value_obj *vo = NULL;
    struct value_obj *old_vo = NULL;
    struct collection_item *item = NULL;
    const char sp_key[] = INI_SECTION_KEY;
    const char *key_ptr;

    TRACE_FLOW_ENTRY();

    /* Check arguments */
    if (!ini_config) {
        TRACE_ERROR_STRING("Invalid argument","ini_config");
        return EINVAL;
    }

    if (!section) {
        TRACE_ERROR_STRING("Invalid argument","section");
        return EINVAL;
    }

    if (!key) {
        TRACE_ERROR_STRING("Invalid argument","key");
        return EINVAL;
    }

    if (!value) {
        TRACE_ERROR_STRING("Invalid argument","value");
        return EINVAL;
    }

    if (idx < 0) {
        TRACE_ERROR_STRING("Invalid argument","idx");
        return EINVAL;
    }


    switch (flags) {

    case INI_VA_NOCHECK:    /* Just fall through */
                            break;

    case INI_VA_MOD:
    case INI_VA_MOD_E:
                            /* Find the value by index.
                             * If value is not found return error.
                             */
                            error = col_get_dup_item(ini_config->cfg,
                                                     section,
                                                     key,
                                                     COL_TYPE_ANY,
                                                     idx,
                                                     EXACT(flags),
                                                     &item);
                            if (error) {
                                TRACE_ERROR_NUMBER("Error "
                                                   "looking for item.",
                                                   error);
                                return error;
                            }
                            break;


    case INI_VA_MODADD:
    case INI_VA_MODADD_E:
                            /* Find the value by index.
                             * If value is not found it is OK.
                             */
                            error = col_get_dup_item(ini_config->cfg,
                                                     section,
                                                     key,
                                                     COL_TYPE_ANY,
                                                     idx,
                                                     EXACT(flags),
                                                     &item);
                            if ((error) && (error != ENOENT)) {
                                TRACE_ERROR_NUMBER("Unexpected error "
                                                   "looking for item.",
                                                   error);
                                return error;
                            }
                            break;



    case INI_VA_DUPERROR:   /* Find any instance */
                            error = col_get_dup_item(ini_config->cfg,
                                                     section,
                                                     key,
                                                     COL_TYPE_ANY,
                                                     0,
                                                     0,
                                                     &item);
                            if ((error) && (error != ENOENT)) {
                                TRACE_ERROR_NUMBER("Unexpected error "
                                                   "looking for item.",
                                                   error);
                                return error;
                            }
                            if (!error) {
                                TRACE_ERROR_NUMBER("Key exists "
                                                   "this is error.",
                                                   error);
                                return EEXIST;
                            }
                            break;
    case INI_VA_CLEAN:      /*  Delete all instaces of the key first */
                            while (!error) {
                                error = col_remove_item(ini_config->cfg,
                                                        section,
                                                        COL_DSP_FIRSTDUP,
                                                        key,
                                                        0,
                                                        COL_TYPE_ANY);
                                if (error) {
                                    if (error != ENOENT) {
                                        TRACE_ERROR_NUMBER("Failed to clean "
                                                           "the section.",
                                                           error);
                                        return error;
                                    }
                                    else break;
                                }
                            }
                            break;
    default:                /* The new ones should be added here */
                            TRACE_ERROR_NUMBER("Flag is not implemented",
                                               ENOSYS);
                            return ENOSYS;
    }

    /* Start with the comment */
    if (comments) {
        error = ini_comment_construct(comments,
                                      count_comment,
                                      &ic);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to construct comment", error);
            return error;
        }
    }

    /* Create value object */
    error =  value_create_new(value,
                              strnlen(value, MAX_VALUE -1),
                              INI_VALUE_CREATED,
                              strnlen(key, MAX_KEY -1),
                              border,
                              ic,
                              &vo);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to construct value object.", error);
        ini_comment_destroy(ic);
        return error;
    }

    if (item) {
        /* If we have selected item remove old value */
        old_vo = *((struct value_obj **)(col_get_item_data(item)));
        value_destroy(old_vo);
        /* Update the item with the new value */
        error =  col_modify_binary_item(item,
                                        NULL,
                                        &vo,
                                        sizeof(struct value_obj *));
        if (error) {
            TRACE_ERROR_NUMBER("Failed to update item.", error);
            value_destroy(vo);
            return error;
        }
    }
    else {
        if (position == COL_DSP_FRONT) {
            key_ptr = sp_key;
            position = COL_DSP_AFTER;
        }
        else {
            key_ptr = other_key;
        }
        /* Add value to collection */
        error = col_insert_binary_property(ini_config->cfg,
                                           section,
                                           position,
                                           key_ptr,
                                           idx,
                                           flags,
                                           key,
                                           &vo,
                                           sizeof(struct value_obj *));
    }
    if (error) {
        TRACE_ERROR_NUMBER("Failed to insert value.", error);
        value_destroy(vo);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Function to add section */
int ini_config_add_section(struct ini_cfgobj *ini_config,
                           const char *section,
                           const char *comments[],
                           size_t count_comment,
                           int position,
                           const char *other_section,
                           int idx)
{
    int error = EOK;
    struct ini_comment *ic = NULL;
    struct value_obj *vo = NULL;
    struct collection_item *item = NULL;
    struct collection_item *sec = NULL;

    TRACE_FLOW_ENTRY();

    /* Check arguments */
    if (!ini_config) {
        TRACE_ERROR_STRING("Invalid argument","ini_config");
        return EINVAL;
    }

    if (!section) {
        TRACE_ERROR_STRING("Invalid argument","section");
        return EINVAL;
    }

    if (position > COL_DSP_INDEX) {
        TRACE_ERROR_STRING("Invalid argument","position");
        return EINVAL;
    }

    if (idx < 0) {
        TRACE_ERROR_STRING("Invalid argument","idx");
        return EINVAL;
    }

    /* Check if section exists */
    error = col_get_item(ini_config->cfg,
                         section,
                         COL_TYPE_COLLECTIONREF,
                         COL_TRAVERSE_ONELEVEL,
                         &item);
    if (error) {
        TRACE_ERROR_NUMBER("Search for section failed.", error);
        return error;
    }

    if (item) {
        TRACE_ERROR_STRING("Section already exists.", section);
        return EEXIST;
    }

    /* Create a new section */
    error = col_create_collection(&sec,
                                  section,
                                  COL_CLASS_INI_SECTION);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to create a section", error);
        return error;
    }

    /* Process comment */
    if (comments) {
        error = ini_comment_construct(comments,
                                      count_comment,
                                      &ic);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to construct comment", error);
            col_destroy_collection(sec);
            return error;
        }
    }

    /* Create value object */
    error =  value_create_new(section,
                              strnlen(section, MAX_VALUE -1),
                              INI_VALUE_CREATED,
                              strlen(INI_SECTION_KEY),
                              INI_WRAP_BOUNDARY,
                              ic,
                              &vo);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to construct value object.", error);
        ini_comment_destroy(ic);
        col_destroy_collection(sec);
        return error;
    }
    /* Comment is now a part of value - no need to clean it seprately. */

    /* Add value to section collection */
    error = col_insert_binary_property(sec,
                                       NULL,
                                       COL_DSP_END,
                                       NULL,
                                       0,
                                       COL_INSERT_NOCHECK,
                                       INI_SECTION_KEY,
                                       &vo,
                                       sizeof(struct value_obj *));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to add value object to section.", error);
        value_destroy(vo);
        col_destroy_collection(sec);
        return error;
    }

    /* Embed section collection */
    /* Since there is no function to do addition of collection to
     * collection with disposition we will use a workaround.
     */

    error = col_insert_property_with_ref(ini_config->cfg,
                                         NULL,
                                         position,
                                         other_section,
                                         idx,
                                         COL_INSERT_NOCHECK,
                                         section,
                                         COL_TYPE_COLLECTIONREF,
                                         (void *)(&sec),
                                         sizeof(struct collection_item **),
                                         NULL);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to embed section", error);
        value_destroy(vo);
        col_destroy_collection(sec);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Function to add or remove comments for a section */
int ini_config_comment_section(struct ini_cfgobj *ini_config,
                               const char *section,
                               const char *comments[],
                               size_t count_comment)
{
    int error = EOK;
    struct ini_comment *ic = NULL;
    struct value_obj *vo = NULL;
    struct collection_item *item = NULL;
    struct collection_item *sec = NULL;

    TRACE_FLOW_ENTRY();

    /* Check arguments */
    if (!ini_config) {
        TRACE_ERROR_STRING("Invalid argument","ini_config");
        return EINVAL;
    }

    if (!section) {
        TRACE_ERROR_STRING("Invalid argument","section");
        return EINVAL;
    }

    /* Get the section */
    error = col_get_item(ini_config->cfg,
                         section,
                         COL_TYPE_COLLECTIONREF,
                         COL_TRAVERSE_ONELEVEL,
                         &item);
    if (error) {
        TRACE_ERROR_NUMBER("Search for section failed.", error);
        return error;
    }

    /* If item not found return error */
    if (!item) {
        TRACE_ERROR_NUMBER("Item not found.", ENOENT);
        return ENOENT;
    }

    /* Item is actually a section reference */
    sec = *((struct collection_item **)col_get_item_data(item));
    item = NULL;

    /* Now get the special item from the section collection */
    error = col_get_item(sec,
                         INI_SECTION_KEY,
                         COL_TYPE_ANY,
                         COL_TRAVERSE_ONELEVEL,
                         &item);
    if (error) {
        TRACE_ERROR_NUMBER("Search for section failed.", error);
        return error;
    }

    /* If item not found return error */
    if (!item) {
        /* Something is really broken with the internal implementation
         * if we can't find the item, thus EINVAL.
         */
        TRACE_ERROR_NUMBER("Item not found.", EINVAL);
        return EINVAL;
    }

    /* Item is actually a value object. */
    vo = *((struct value_obj **)(col_get_item_data(item)));

    /* Build comment */
    if (comments) {
        error = ini_comment_construct(comments,
                                      count_comment,
                                      &ic);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to construct comment", error);
            return error;
        }
    }

    /* Replace comment with the new one. Old one is freed by the function */
    error = value_put_comment(vo, ic);
    if (error) {
        TRACE_ERROR_NUMBER("Faile to update comment.",
                            error);
        ini_comment_destroy(ic);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}


/* Function to rename section */
int ini_config_rename_section(struct ini_cfgobj *ini_config,
                              const char *section,
                              const char *newname)
{
    int error = EOK;
    struct collection_item *item = NULL;
    struct collection_item *sec = NULL;

    TRACE_FLOW_ENTRY();

    /* Check arguments */
    if (!ini_config) {
        TRACE_ERROR_STRING("Invalid argument","ini_config");
        return EINVAL;
    }

    if (!section) {
        TRACE_ERROR_STRING("Invalid argument","section");
        return EINVAL;
    }

    if (!newname) {
        TRACE_ERROR_STRING("Invalid argument","newname");
        return EINVAL;
    }

    /* Get the section */
    error = col_get_item(ini_config->cfg,
                         section,
                         COL_TYPE_COLLECTIONREF,
                         COL_TRAVERSE_ONELEVEL,
                         &item);
    if (error) {
        TRACE_ERROR_NUMBER("Search for section failed.", error);
        return error;
    }

    /* If item not found return error */
    if (!item) {
        TRACE_ERROR_NUMBER("Item not found.", ENOENT);
        return ENOENT;
    }

    /* Item is actually a section reference */
    sec = *((struct collection_item **)col_get_item_data(item));

    /* Change name only */
    error = col_modify_item(item,
                            newname,
                            0,
                            NULL,
                            0);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to change section name.", error);
        return error;
    }

    /* Change name of the embedded collection (for consistency) */
    error = col_modify_item(sec,
                            newname,
                            0,
                            NULL,
                            0);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to change section name "
                           "inside the embedded collection.", error);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Cleanup collback */
static void cb(const char *property,
               int property_len,
               int type,
               void *data,
               int length,
               void *ext_data)
{
    struct value_obj *vo;
    TRACE_FLOW_ENTRY();

    if ((type == COL_TYPE_COLLECTIONREF) ||
        (type == COL_TYPE_COLLECTION)) return;

    vo = *((struct value_obj **)(data));
    value_destroy(vo);

    TRACE_FLOW_EXIT();

}

/* Function to delete section by name */
int ini_config_delete_section_by_name(struct ini_cfgobj *ini_config,
                                      const char *section)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();
    error = ini_config_delete_section_by_position(ini_config,
                                                  COL_DSP_FIRSTDUP,
                                                  section,
                                                  0);
    if (error) {
        TRACE_ERROR_NUMBER("Search for section failed.", error);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Function to delete section by relative postion */
int ini_config_delete_section_by_position(struct ini_cfgobj *ini_config,
                                          int position,
                                          const char *section,
                                          int idx)
{
    int error = EOK;
    struct collection_item *item = NULL;

    TRACE_FLOW_ENTRY();

    /* Check arguments */
    if (!ini_config) {
        TRACE_ERROR_STRING("Invalid argument","ini_config");
        return EINVAL;
    }

    if (!section) {
        TRACE_ERROR_STRING("Invalid argument","section");
        return EINVAL;
    }

    if (idx < 0) {
        TRACE_ERROR_STRING("Invalid argument","idx");
        return EINVAL;
    }

    /* Extract section */
    error = col_extract_item(ini_config->cfg,
                             NULL,
                             position,
                             section,
                             idx,
                             COL_TYPE_ANY,
                             &item);
    if (error) {
        TRACE_ERROR_NUMBER("Search for section failed.", error);
        return error;
    }

    /* If item not found return error */
    if (!item) {
        TRACE_ERROR_NUMBER("Item not found.", ENOENT);
        return ENOENT;
    }

    /* Delete item and subcollection */
    col_delete_item_with_cb(item, cb, NULL);

    TRACE_FLOW_EXIT();
    return EOK;
}
