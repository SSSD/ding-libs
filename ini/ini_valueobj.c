/*
    INI LIBRARY

    Module represents interface to the value object.

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
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "simplebuffer.h"
#include "ref_array.h"
#include "ini_comment.h"
#include "ini_defines.h"
#include "ini_valueobj.h"
#include "trace.h"

struct value_obj {
    struct ref_array *raw_lines;
    struct ref_array *raw_lengths;
    struct simplebuffer *unfolded;
    uint32_t origin;
    uint32_t line;
    uint32_t keylen;
    uint32_t boundary;
    struct ini_comment *ic;
};

/* The length of " =" which is 3 */
#define INI_FOLDING_OVERHEAD 3

/* Array growth */
#define INI_ARRAY_GROW  2

/* Equal sign */
#define INI_EQUAL_SIGN  " = "
#define INI_OPEN_BR     "["
#define INI_CLOSE_BR    "]"


/* Unfold the value represented by the array */
static int value_unfold(struct ref_array *raw_lines,
                        struct ref_array *raw_lengths,
                        struct simplebuffer **unfolded)
{
    int error;
    struct simplebuffer *oneline = NULL;
    uint32_t len = 0;
    char *ptr = NULL;
    uint32_t i = 0;
    char *part = NULL;

    TRACE_FLOW_ENTRY();

    error = simplebuffer_alloc(&oneline);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    for (;;) {
        /* Get line */
        ptr = ref_array_get(raw_lines, i, NULL);
        if (ptr) {
            /* Get its length */
            ref_array_get(raw_lengths, i, (void *)&len);

            part = *((char **)(ptr));

            TRACE_INFO_STRING("Value:", part);
            TRACE_INFO_NUMBER("Lenght:", len);

            error = simplebuffer_add_raw(oneline,
                                         part,
                                         len,
                                         INI_VALUE_BLOCK);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to add string", error);
                simplebuffer_free(oneline);
                return error;
            }

            i++;
        }
        else break;
    }

    *unfolded = oneline;

    TRACE_FLOW_EXIT();
    return error;
}


static int save_portion(struct ref_array *raw_lines,
                        struct ref_array *raw_lengths,
                        const char* buf,
                        uint32_t len)
{
    int error = EOK;
    char *copy = NULL;
    uint32_t adj = 0;

    TRACE_FLOW_ENTRY();

    /* Add leading space only if there is
     * a) no space
     * b) it is not an empty line
     * c) it is now a first line
     */

    if ((buf[0] != ' ') &&
        (buf[0] != '\t') &&
        (len != 0) &&
        (ref_array_len(raw_lines) != 0)) adj = 1;

    copy = malloc(len + adj + 1);
    if (!copy) {
        TRACE_ERROR_NUMBER("Failed to allocate memory", ENOMEM);
        return ENOMEM;
    }

    memcpy(copy + adj, buf, len);
    len += adj;
    copy[len] = 0;

    /* If the section being saved is not starting
     * with space add a space.
     */
    if (adj) copy[0] = ' ';

    error = ref_array_append(raw_lines, (void *)(&copy));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to append line",
                            error);
        free(copy);
        return error;
    }

    error = ref_array_append(raw_lengths, (void *)(&len));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to append length",
                            error);
        return error;
    }

    TRACE_INFO_STRING("Added string:", (char *)copy);
    TRACE_INFO_NUMBER("Added number:", len);


    TRACE_FLOW_EXIT();
    return EOK;
}

/* Function to create a folded value out of the unfolded string */
static int value_fold(struct simplebuffer *unfolded,
                      uint32_t key_len,
                      uint32_t fold_bound,
                      struct ref_array *raw_lines,
                      struct ref_array *raw_lengths)
{
    int error = EOK;
    const char *buf;
    uint32_t len = 0;          /* Full length of the buffer          */
    uint32_t fold_place = 0;   /* Potential folding place            */
    uint32_t best_place = 0;   /* Dynamic folding boundary           */
    uint32_t next_place = 0;   /* Position of the found space        */
    uint32_t fold_len = 0;     /* Determined length of the substring */
    uint32_t idx = 0;          /* Counter of lines                   */
    uint32_t i = 0;            /* Internal counter                   */
    uint32_t resume_place = 0; /* Place we resume parsing            */
    uint32_t start_place = 0;  /* Start of the string                */
    int done = 0;              /* Are we done?                       */

    TRACE_FLOW_ENTRY();

    /* Reset arrays */
    ref_array_reset(raw_lines);
    ref_array_reset(raw_lengths);

    /* Get the buffer info */
    len = simplebuffer_get_len(unfolded);
    if (!len) {
        /* Nothing to fold */
        TRACE_FLOW_EXIT();
        return EOK;
    }

    buf = (const char *)simplebuffer_get_buf(unfolded);

    TRACE_INFO_STRING("Unfolded value:", buf);

    /* Make sure that we have at least one character to fold */
    if (fold_bound == 0) fold_bound++;

    while (!done) {
        /* Determine the max length of the line */
        if (idx == 0) {
             if (fold_bound > (key_len + INI_FOLDING_OVERHEAD)) {
                 best_place = fold_bound - key_len - INI_FOLDING_OVERHEAD;
             }
             else best_place = 0;
        }
        else {
             best_place = fold_bound;

             /* Starting with the second line if we plan
              * to add space ourselves factor it into folding
              * boadary
              */
             if ((buf[start_place] != ' ') &&
                 (buf[start_place] != '\t')) best_place--;
        }

        TRACE_INFO_NUMBER("Best place", best_place);

        fold_place = start_place;
        next_place = start_place;
        best_place += start_place;


        /* Parse the buffer from the right place */
        for (i = resume_place; i <= len; i++) {

            /* Check for folding opportunity */
            if (i == len) {
                next_place = i;
                done = 1;
            }
            /*
             * Fold if we found the separator or the first line
             * is too long right away
             */
            else if (((buf[i] == ' ') || (buf[i] == '\t')) ||
                     ((best_place == 0) && (i == 0))) {
                next_place = i;
                TRACE_INFO_NUMBER("Next place:", next_place);
            }
            else continue;

            if ((next_place > best_place) || (next_place == 0)) {
                if ((fold_place == start_place) &&
                    (next_place != 0)) {
                    /* Our first found folding place
                     * is already after the preferred
                     * folding place. Time to fold then...
                     */
                    fold_len = next_place - start_place;

                }
                else {
                    /* We will use the previous
                     * folding place.
                     */
                    fold_len = fold_place - start_place;

                }

                TRACE_INFO_NUMBER("Fold len:", fold_len);

                error = save_portion(raw_lines,
                                     raw_lengths,
                                     buf + start_place,
                                     fold_len);
                if (error) {
                    TRACE_ERROR_NUMBER("Failed to save", error);
                    return error;
                }

                start_place += fold_len;

                /*
                 * This will force the re-processing
                 * of the same space but it is
                 * helpful in case the middle portion
                 * of the value is beyond our folding limit.
                 */
                resume_place = next_place;
                if (fold_len == 0) resume_place++;
                idx++;
                break;
            }
            else { /* Case when next_place <= best_place */
                fold_place = next_place;
            }
        }

        /* Save last portion */
        if (done) {
            if (next_place - start_place) {
                error = save_portion(raw_lines,
                                     raw_lengths,
                                     buf + start_place,
                                     next_place - start_place);
                if (error) {
                    TRACE_ERROR_NUMBER("Failed to save last chunk", error);
                    return error;
                }
                idx++;
            }
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}


/* Create value from a referenced array */
int value_create_from_refarray(struct ref_array *raw_lines,
                               struct ref_array *raw_lengths,
                               uint32_t line,
                               uint32_t origin,
                               uint32_t key_len,
                               uint32_t boundary,
                               struct ini_comment *ic,
                               struct value_obj **vo)
{
    int error = EOK;
    struct value_obj *new_vo = NULL;

    TRACE_FLOW_ENTRY();

    if ((!raw_lines) || (!raw_lengths) || (!vo)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    new_vo = malloc(sizeof(struct value_obj));
    if (!new_vo) {
        TRACE_ERROR_NUMBER("No memory", ENOMEM);
        return ENOMEM;
    }

    /* We are not using references here since
     * it will be inconsistent with the way
     * how comment is handled.
     * We could have added references here and make
     * comment keep references but it seems to be
     * and overhead in this case.
     */
    new_vo->raw_lines = raw_lines;
    new_vo->raw_lengths = raw_lengths;
    new_vo->origin = origin;
    new_vo->line = line;
    new_vo->keylen = key_len;
    new_vo->boundary = boundary;
    new_vo->ic = ic;

    error = value_unfold(new_vo->raw_lines,
                         new_vo->raw_lengths,
                         &(new_vo->unfolded));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to unfold", error);
        value_destroy(new_vo);
        return error;
    }

    TRACE_INFO_STRING("Unfolded:",
                      (const char *)simplebuffer_get_buf(new_vo->unfolded));
    *vo = new_vo;

    TRACE_FLOW_EXIT();

    return error;
}

/* Cleanup callback for lines array */
void value_lines_cleanup_cb(void *elem,
                            ref_array_del_enum type,
                            void *data)
{
    char *part;

    TRACE_FLOW_ENTRY();

    part = *((char **)(elem));

    TRACE_INFO_STRING("Freeing:", part);

    free(part);

    TRACE_FLOW_EXIT();
}

/* Create a pair of arrays */
int value_create_arrays(struct ref_array **raw_lines,
                        struct ref_array **raw_lengths)
{
    int error = EOK;
    struct ref_array *new_lines = NULL;
    struct ref_array *new_lengths = NULL;

    TRACE_FLOW_ENTRY();

    error = ref_array_create(&new_lines,
                             sizeof(char *),
                             INI_ARRAY_GROW,
                             value_lines_cleanup_cb,
                             NULL);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to create lines array", error);
        return error;

    }

    error = ref_array_create(&new_lengths,
                             sizeof(uint32_t),
                             INI_ARRAY_GROW,
                             NULL,
                             NULL);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to create lengths array", error);
        ref_array_destroy(new_lines);
        return error;

    }

    *raw_lines = new_lines;
    *raw_lengths = new_lengths;

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Add a raw string to the arrays */
int value_add_to_arrays(const char *strvalue,
                        uint32_t len,
                        struct ref_array *raw_lines,
                        struct ref_array *raw_lengths)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ref_array_append(raw_lines, (void *)(&strvalue));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to add to lines array", error);
        return error;

    }

    error = ref_array_append(raw_lengths, (void *)(&len));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to add to lengths array", error);
        return error;

    }

    TRACE_FLOW_EXIT();
    return error;
}


/* Destroy arrays */
void value_destroy_arrays(struct ref_array *raw_lines,
                          struct ref_array *raw_lengths)
{
    TRACE_FLOW_ENTRY();

    /* Function checks validity inside */
    ref_array_destroy(raw_lines);
    /* Function checks validity inside */
    ref_array_destroy(raw_lengths);

    TRACE_FLOW_EXIT();

}

/* Destroy a value object */
void value_destroy(struct value_obj *vo)
{
    TRACE_FLOW_ENTRY();

    if (vo) {
        /* Free arrays if any */
        value_destroy_arrays(vo->raw_lines,
                             vo->raw_lengths);
        /* Free the simple buffer if any */
        simplebuffer_free(vo->unfolded);
        /* Function checks validity inside */
        ini_comment_destroy(vo->ic);
        free(vo);
    }

    TRACE_FLOW_EXIT();
}

/* Create value object from string buffer */
int value_create_new(const char *strvalue,
                     uint32_t length,
                     uint32_t origin,
                     uint32_t key_len,
                     uint32_t boundary,
                     struct ini_comment *ic,
                     struct value_obj **vo)
{
    int error = EOK;
    struct value_obj *new_vo = NULL;
    struct simplebuffer *oneline = NULL;

    TRACE_FLOW_ENTRY();

    if ((!strvalue) || (!vo)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    /* Create buffer to hold the value */
    error = simplebuffer_alloc(&oneline);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    /* Put value into the buffer */
    error = simplebuffer_add_str(oneline,
                                 strvalue,
                                 length,
                                 INI_VALUE_BLOCK);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to add string", error);
        simplebuffer_free(oneline);
        return error;
    }

    /* Acllocate new INI value structure */
    new_vo = malloc(sizeof(struct value_obj));
    if (!new_vo) {
        TRACE_ERROR_NUMBER("No memory", ENOMEM);
        simplebuffer_free(oneline);
        return ENOMEM;
    }

    new_vo->origin = origin;
    /* Line is not known in this case */
    new_vo->line = 0;
    new_vo->ic = ic;
    new_vo->unfolded = oneline;
    new_vo->keylen = key_len;
    new_vo->boundary = boundary;
    new_vo->raw_lines = NULL;
    new_vo->raw_lengths = NULL;

    error = value_create_arrays(&(new_vo->raw_lines),
                                &(new_vo->raw_lengths));

    if (error) {
        TRACE_ERROR_NUMBER("Failed to fold", error);
        value_destroy(new_vo);
        return error;
    }

    /* Create arrays by folding the value */
    error = value_fold(new_vo->unfolded,
                       new_vo->keylen,
                       new_vo->boundary,
                       new_vo->raw_lines,
                       new_vo->raw_lengths);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to fold", error);
        value_destroy(new_vo);
        return error;
    }

    *vo = new_vo;

    TRACE_FLOW_EXIT();

    return error;
}

/* Create a copy of the value */
int value_copy(struct value_obj *vo,
               struct value_obj **copy_vo)
{

    int error = EOK;
    struct value_obj *new_vo = NULL;
    struct simplebuffer *oneline = NULL;

    TRACE_FLOW_ENTRY();

    if ((!copy_vo) || (!vo)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    /* Create buffer to hold the value */
    error = simplebuffer_alloc(&oneline);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    /* Put value into the buffer */
    error = simplebuffer_add_str(oneline,
                                 (const char *)simplebuffer_get_buf(vo->unfolded),
                                 simplebuffer_get_len(vo->unfolded),
                                 INI_VALUE_BLOCK);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to add string", error);
        simplebuffer_free(oneline);
        return error;
    }

    /* Acllocate new INI value structure */
    new_vo = malloc(sizeof(struct value_obj));
    if (!new_vo) {
        TRACE_ERROR_NUMBER("No memory", ENOMEM);
        simplebuffer_free(oneline);
        return ENOMEM;
    }

    new_vo->origin = vo->origin;
    new_vo->line = vo->line;
    new_vo->unfolded = oneline;
    new_vo->keylen = vo->keylen;
    new_vo->boundary = vo->boundary;
    new_vo->raw_lines = NULL;
    new_vo->raw_lengths = NULL;
    new_vo->ic = NULL;

    error = value_create_arrays(&(new_vo->raw_lines),
                                &(new_vo->raw_lengths));

    if (error) {
        TRACE_ERROR_NUMBER("Failed to fold", error);
        value_destroy(new_vo);
        return error;
    }

    /* Create arrays by folding the value */
    error = value_fold(new_vo->unfolded,
                       new_vo->keylen,
                       new_vo->boundary,
                       new_vo->raw_lines,
                       new_vo->raw_lengths);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to fold", error);
        value_destroy(new_vo);
        return error;
    }

    /* Copy comment */
    if (vo->ic) {
        error = ini_comment_copy(vo->ic, &new_vo->ic);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to copy comment", error);
            value_destroy(new_vo);
            return error;
        }
    }

    *copy_vo = new_vo;

    TRACE_INFO_STRING("Orig value:",
                      (const char *)simplebuffer_get_buf(vo->unfolded));
    TRACE_INFO_STRING("Copy value:",
                      (const char *)simplebuffer_get_buf(new_vo->unfolded));

    TRACE_INFO_NUMBER("Orig value num lines:",
                      ref_array_len(vo->raw_lengths));
    TRACE_INFO_NUMBER("Copy value num lines:",
                      ref_array_len(new_vo->raw_lengths));

    TRACE_FLOW_EXIT();
    return error;
}

/* Get concatenated value */
int value_get_concatenated(struct value_obj *vo,
                           const char **fullstr)
{
    TRACE_FLOW_ENTRY();

    if (!vo) {
        TRACE_ERROR_NUMBER("Invalid object", EINVAL);
        return EINVAL;
    }

    if (!fullstr)
    {
        TRACE_ERROR_NUMBER("Invalid output value", EINVAL);
        return EINVAL;
    }

    *fullstr = (const char *)simplebuffer_get_buf(vo->unfolded);

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Get length of the concatenated value */
int value_get_concatenated_len(struct value_obj *vo,
                               uint32_t *len)
{
    TRACE_FLOW_ENTRY();

    if (!vo) {
        TRACE_ERROR_NUMBER("Invalid object", EINVAL);
        return EINVAL;
    }

    if (!len)
    {
        TRACE_ERROR_NUMBER("Invalid output value", EINVAL);
        return EINVAL;
    }

    *len = simplebuffer_get_len(vo->unfolded);

    TRACE_FLOW_EXIT();
    return EOK;
}


/* Get value's origin */
int value_get_origin(struct value_obj *vo, uint32_t *origin)
{
    TRACE_FLOW_ENTRY();

    if (!vo) {
        TRACE_ERROR_NUMBER("Invalid object", EINVAL);
        return EINVAL;
    }

    if (!origin)
    {
        TRACE_ERROR_NUMBER("Invalid output value", EINVAL);
        return EINVAL;
    }

    *origin = vo->origin;

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Get value's line */
int value_get_line(struct value_obj *vo, uint32_t *line)
{
    TRACE_FLOW_ENTRY();

    if (!vo) {
        TRACE_ERROR_NUMBER("Invalid object", EINVAL);
        return EINVAL;
    }

    if (!line)
    {
        TRACE_ERROR_NUMBER("Invalid output value", EINVAL);
        return EINVAL;
    }

    *line = vo->line;

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Update key length */
int value_set_keylen(struct value_obj *vo, uint32_t key_len)
{
    int error = EOK;
    TRACE_FLOW_ENTRY();

    if (!vo) {
        TRACE_ERROR_NUMBER("Invalid object", EINVAL);
        return EINVAL;
    }

    vo->keylen = key_len;

    /* Fold in new value */
    error = value_fold(vo->unfolded,
                       vo->keylen,
                       vo->boundary,
                       vo->raw_lines,
                       vo->raw_lengths);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to fold", error);
        /* In this case nothing to free here but
         * the object might be unsiable */
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Change boundary */
int value_set_boundary(struct value_obj *vo, uint32_t boundary)
{
    int error = EOK;
    TRACE_FLOW_ENTRY();

    if (!vo) {
        TRACE_ERROR_NUMBER("Invalid object", EINVAL);
        return EINVAL;
    }

    vo->boundary = boundary;

    /* Fold in new value */
    error = value_fold(vo->unfolded,
                       vo->keylen,
                       vo->boundary,
                       vo->raw_lines,
                       vo->raw_lengths);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to fold", error);
        /* In this case nothing to free here but
         * the object might be unusable */
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Update value */
int value_update(struct value_obj *vo,
                 const char *value,
                 uint32_t length,
                 uint32_t origin,
                 uint32_t boundary)
{
    int error = EOK;
    struct simplebuffer *oneline = NULL;

    if ((!value) || (!vo)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    /* Create buffer to hold the value */
    error = simplebuffer_alloc(&oneline);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    /* Put value into the buffer */
    error = simplebuffer_add_str(oneline,
                                 value,
                                 length,
                                 INI_VALUE_BLOCK);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to add string", error);
        simplebuffer_free(oneline);
        return error;
    }

    simplebuffer_free(vo->unfolded);

    vo->origin = origin;
    vo->unfolded = oneline;
    vo->boundary = boundary;

    /* Fold in new value */
    error = value_fold(vo->unfolded,
                       vo->keylen,
                       vo->boundary,
                       vo->raw_lines,
                       vo->raw_lengths);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to fold", error);
        /* In this case nothing to free here but
         * the object might be unsiable */
        return error;
    }

    TRACE_FLOW_EXIT();

    return error;

}

/* Get comment from the value */
int value_extract_comment(struct value_obj *vo, struct ini_comment **ic)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if ((!vo) || (!ic)) {
        TRACE_ERROR_NUMBER("Invalid input parameter", EINVAL);
        return EINVAL;
    }

    *ic = vo->ic;
    vo->ic = NULL;

    TRACE_FLOW_EXIT();
    return error;

}

/* Set comment into the value */
int value_put_comment(struct value_obj *vo, struct ini_comment *ic)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if ((!vo) || (!ic)) {
        TRACE_ERROR_NUMBER("Invalid input parameter", EINVAL);
        return EINVAL;
    }

    if (vo->ic != ic) {
        /* Remove existing comment if any */
        ini_comment_destroy(vo->ic);
    }

    vo->ic = ic;

    TRACE_FLOW_EXIT();
    return error;

}

/* Serialize value */
int value_serialize(struct value_obj *vo,
                    const char *key,
                    struct simplebuffer *sbobj)
{
    int error = EOK;
    uint32_t i = 0;
    uint32_t len = 0;
    char *ptr = NULL;
    char *part = NULL;
    int sec = 0;
    uint32_t vln = 0;

    TRACE_FLOW_ENTRY();
    TRACE_INFO_STRING("Serializing key:", key);

    if (!vo) {
        TRACE_ERROR_NUMBER("Invalid input parameter", EINVAL);
        return EINVAL;
    }

    /* Put comment first */
    if (vo->ic) {
        error = ini_comment_serialize(vo->ic, sbobj);
        if (error) {
            TRACE_ERROR_NUMBER("Failed serialize comment", error);
            return error;
        }
    }

    if (strncmp(key, INI_SPECIAL_KEY, sizeof(INI_SPECIAL_KEY)) == 0) {
        /* Special key carries only a comment */
        TRACE_FLOW_EXIT();
        return EOK;
    }

    /* Handle the case it is a section key */
    if (strncmp(key,
                INI_SECTION_KEY,
                sizeof(INI_SECTION_KEY)) == 0) sec = 1;

    if (sec) {
        error = simplebuffer_add_str(sbobj,
                                     INI_OPEN_BR,
                                     sizeof(INI_OPEN_BR) - 1,
                                     INI_VALUE_BLOCK);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to add opening section bracket", error);
            return error;

        }
    }
    else {

        error = simplebuffer_add_str(sbobj,
                                     key,
                                     vo->keylen,
                                     INI_VALUE_BLOCK);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to add key", error);
            return error;
        }

        error = simplebuffer_add_str(sbobj,
                                     INI_EQUAL_SIGN,
                                     sizeof(INI_EQUAL_SIGN) - 1,
                                     INI_VALUE_BLOCK);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to add equal sign", error);
            return error;
        }

    }

    if (vo->raw_lines) {

        vln = ref_array_len(vo->raw_lines);
        TRACE_INFO_NUMBER("Number of lines:", vln);

#ifdef HAVE_TRACE

extern void ref_array_debug(struct ref_array *ra, int num);

        ref_array_debug(vo->raw_lines, 0);
        ref_array_debug(vo->raw_lengths, 1);
#endif

        for (i = 0; i < vln; i++) {
            /* Get line */
            ptr = ref_array_get(vo->raw_lines, i, NULL);

            if (ptr) {
                /* Get its length */
                len = 0;
                ref_array_get(vo->raw_lengths, i, (void *)&len);

                part = *((char **)(ptr));

                TRACE_INFO_STRING("Value:", part);
                TRACE_INFO_NUMBER("Lenght:", len);

                error = simplebuffer_add_raw(sbobj,
                                             part,
                                             len,
                                             INI_VALUE_BLOCK);
                if (error) {
                    TRACE_ERROR_NUMBER("Failed to add value", error);
                    return error;
                }

            }
            if (!sec) {
                error = simplebuffer_add_cr(sbobj);
                if (error) {
                    TRACE_ERROR_NUMBER("Failed to add CR", error);
                    return error;
                }
            }
        }

        if ((!vln) && (!sec)) {
            error = simplebuffer_add_cr(sbobj);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to add CR", error);
                return error;
            }
        }
    }

    if (sec) {
        error = simplebuffer_add_str(sbobj,
                                     INI_CLOSE_BR,
                                     sizeof(INI_CLOSE_BR) - 1,
                                     INI_VALUE_BLOCK);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to add closing bracket", error);
            return error;

        }

        error = simplebuffer_add_cr(sbobj);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to add CR", error);
            return error;
        }
    }

    TRACE_INFO_STRING("Buffer:", (const char *)simplebuffer_get_buf(sbobj));
    TRACE_FLOW_EXIT();
    return error;
}

/* Merge comment from one value into another */
int value_merge_comment(struct value_obj *vo_donor,
                        struct value_obj *vo)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if ((!vo) || (!vo_donor)) {
        TRACE_ERROR_NUMBER("Invalid input parameter", EINVAL);
        return EINVAL;
    }

    if (vo_donor->ic) {

        /* If there is something to add */

        if (vo->ic) {

            /* Merge comments if both present */
            error = ini_comment_add(vo_donor->ic, vo->ic);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to merge the comment", error);
                return error;
            }
        }
        else {

            /* Copy comment if only donor present */
            error = ini_comment_copy(vo_donor->ic, &(vo->ic));
            if (error) {
                TRACE_ERROR_NUMBER("Failed to merge the comment", error);
                return error;
            }
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}


/* Print value */
void value_print(const char *key, struct value_obj *vo)
{

    int error = EOK;
    struct simplebuffer *sbobj = NULL;

    TRACE_FLOW_ENTRY();

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        printf("Failed to allocate dynamic string %d.\n", error);
        return;
    }

    /* Serialize */
    error = value_serialize(vo, key, sbobj);
    if (error) {
        printf("Failed to serialize a value object %d.\n", error);
        simplebuffer_free(sbobj);
        return;
    }

    printf("%s", simplebuffer_get_buf(sbobj));
    simplebuffer_free(sbobj);

    TRACE_FLOW_EXIT();
}
