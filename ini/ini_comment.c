/*
    INI LIBRARY

    Object to handle comments

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
#include <ctype.h>
#include "trace.h"
#include "ref_array.h"
#include "simplebuffer.h"
#include "ini_comment.h"
#include "ini_defines.h"

/* The lines will increment in this number */
#define INI_COMMENT_BLOCK 10
/* Default comment length */
#define INI_COMMENT_LEN 100


/***************************/
/* Internal comment states */
/***************************/
/* Empty - initial */
#define INI_COMMENT_EMPTY   0
/* Read - read from file */
#define INI_COMMENT_READ    1
/* Comment was altered */
#define INI_COMMENT_CHANGED 2


/*********************************/
/* Modes to wrap ref array calls */
/*********************************/
#define INI_COMMENT_MODE_BUILD      1
#define INI_COMMENT_MODE_APPEND     2
#define INI_COMMENT_MODE_INSERT     3
#define INI_COMMENT_MODE_REPLACE    4
#define INI_COMMENT_MODE_REMOVE     5
#define INI_COMMENT_MODE_CLEAR      6

/****************************************/
/* Internal structure to hold a comment */
/****************************************/
struct ini_comment {
    struct ref_array *ra;
    uint32_t state;
};


/****************************************/

/* Destroy the comment object */
void ini_comment_destroy(struct ini_comment *ic)
{

    TRACE_FLOW_ENTRY();
    if (ic) {
        /* Function will check for NULL */
        ref_array_destroy(ic->ra);
        free(ic);
    }
    TRACE_FLOW_EXIT();
}


/* Cleanup callback */
static void ini_comment_cb(void *elem,
                           ref_array_del_enum type,
                           void *data)
{

    TRACE_FLOW_ENTRY();
    simplebuffer_free(*((struct simplebuffer **)elem));
    TRACE_FLOW_EXIT();
}


/* Create a comment object */
int ini_comment_create(struct ini_comment **ic)
{
    int error = EOK;
    struct ref_array *ra = NULL;
    struct ini_comment *ic_new = NULL;

    TRACE_FLOW_ENTRY();

    error = ref_array_create(&ra,
                             sizeof(struct simplebuffer *),
                             INI_COMMENT_BLOCK,
                             ini_comment_cb,
                             NULL);
    if (error) {
        TRACE_ERROR_NUMBER("Error creating ref array", error);
        return error;
    }

    ic_new = malloc(sizeof(struct ini_comment));
    if (!ic_new) {
        TRACE_ERROR_NUMBER("Memory allocation error", ENOMEM);
        ref_array_destroy(ra);
        return ENOMEM;
    }

    /* Initialize members here */
    ic_new->ra = ra;
    ic_new->state = INI_COMMENT_EMPTY;

    *ic = ic_new;

    TRACE_FLOW_EXIT();
    return error;
}

/* Callback to copy comment */
static int ini_comment_copy_cb(void *elem,
                               void *new_elem)
{
    int error = EOK;
    struct simplebuffer *sb = NULL;
    struct simplebuffer *sb_new = NULL;

    TRACE_FLOW_ENTRY();

    error = simplebuffer_alloc(&sb_new);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate buffer", error);
        return error;
    }

    sb = *((struct simplebuffer **)elem);
    error = simplebuffer_add_str(sb_new,
                                 (const char *)simplebuffer_get_buf(sb),
                                 simplebuffer_get_len(sb),
                                 INI_COMMENT_LEN);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate buffer", error);
        simplebuffer_free(sb_new);
        return error;
    }

    *((struct simplebuffer **)new_elem) = sb_new;

    TRACE_FLOW_EXIT();
    return error;
}


/* Create a copy of the comment object */
int ini_comment_copy(struct ini_comment *ic,
                     struct ini_comment **ic_copy)
{
    int error = EOK;
    struct ref_array *ra = NULL;
    struct ini_comment *ic_new = NULL;

    TRACE_FLOW_ENTRY();

    error = ref_array_copy(ic->ra,
                           ini_comment_copy_cb,
                           ini_comment_cb,
                           NULL,
                           &ra);
    if (error) {
        TRACE_ERROR_NUMBER("Error creating a copy of ref array", error);
        return error;
    }

    ic_new = malloc(sizeof(struct ini_comment));
    if (!ic_new) {
        TRACE_ERROR_NUMBER("Memory allocation error", ENOMEM);
        ref_array_destroy(ra);
        return ENOMEM;
    }

    /* Initialize members here */
    ic_new->ra = ra;
    ic_new->state = ic->state;

    *ic_copy = ic_new;

    TRACE_FLOW_EXIT();
    return error;
}

/*
 * Modify the comment object
 */
static int ini_comment_modify(struct ini_comment *ic,
                              int mode,
                              uint32_t idx,
                              const char *line,
                              uint32_t length)
{
    int error = EOK;
    struct simplebuffer *elem = NULL;
    struct simplebuffer *empty = NULL;
    char *input = NULL;

    uint32_t i, len = 0;
    uint32_t input_len = 0;

    TRACE_FLOW_ENTRY();

    if (!ic) {
        TRACE_ERROR_NUMBER("Invalid comment object", EINVAL);
        return EINVAL;
    }


    if (mode == INI_COMMENT_MODE_BUILD) {
        /*
         * Can use this function only if object is empty or
         * reading from the file.
         */
        if ((ic->state != INI_COMMENT_EMPTY) &&
            (ic->state != INI_COMMENT_READ)) {
            TRACE_ERROR_NUMBER("Invalid use of the function", EINVAL);
            return EINVAL;
        }
    }

    /* Make sure that we ignore "line" in reset case */
    if (mode != INI_COMMENT_MODE_CLEAR)
        memcpy(&input, &line, sizeof(char *));

    if (mode != INI_COMMENT_MODE_REMOVE) {

        error = simplebuffer_alloc(&elem);
        if (error) {
            TRACE_ERROR_NUMBER("Allocate buffer for the comment", error);
            return error;
        }

        if (input) {
            if (length == 0) input_len = strlen(input);
            else input_len = length;

            error = simplebuffer_add_str(elem,
                                         input,
                                         input_len,
                                         INI_COMMENT_LEN);
        }
        else {
            error = simplebuffer_add_str(elem,
                                         "",
                                         0,
                                         INI_COMMENT_LEN);
        }

        if (error) {
            TRACE_ERROR_NUMBER("Allocate buffer for the comment", error);
            simplebuffer_free(elem);
            return error;
        }
    }

    /* Do action depending on mode */
    switch (mode) {
    case INI_COMMENT_MODE_BUILD:

        TRACE_INFO_STRING("BUILD mode", "");
        error = ref_array_append(ic->ra, (void *)&elem);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to append line to an array", error);
            simplebuffer_free(elem);
            return error;
        }

        break;

    case INI_COMMENT_MODE_APPEND:

        TRACE_INFO_STRING("Append mode", "");
        error = ref_array_append(ic->ra, (void *)&elem);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to append line to an array", error);
            simplebuffer_free(elem);
            return error;
        }

        break;

    case INI_COMMENT_MODE_INSERT:

        TRACE_INFO_STRING("Insert mode", "");
        len = ref_array_len(ic->ra);
        if (idx > len) {
            /* Fill in empty lines */
            for (i = 0; i < (idx-len); i++) {
                error = simplebuffer_alloc(&empty);
                if (error) {
                    TRACE_ERROR_NUMBER("Allocate buffer for the comment", error);
                    simplebuffer_free(elem);
                    return error;
                }
                error = simplebuffer_add_str(elem,
                                             NULL,
                                             0,
                                             INI_COMMENT_LEN);
                if (error) {
                    TRACE_ERROR_NUMBER("Make comment empty", error);
                    simplebuffer_free(empty);
                    simplebuffer_free(elem);
                    return error;
                }
                error = ref_array_append(ic->ra, (void *)&empty);
                if (error) {
                    TRACE_ERROR_NUMBER("Append problem", error);
                    simplebuffer_free(empty);
                    simplebuffer_free(elem);
                    return error;
                }
            }
            /* Append last line */
            error = ref_array_append(ic->ra, (void *)&elem);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to append last line", error);
                simplebuffer_free(elem);
                return error;
            }
        }
        else {
            /* Insert inside the array */
            error = ref_array_insert(ic->ra, idx, (void *)&elem);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to append last line", error);
                simplebuffer_free(elem);
                return error;
            }

        }
        break;


    case INI_COMMENT_MODE_REPLACE:

        TRACE_INFO_STRING("Replace mode", "");
        error = ref_array_replace(ic->ra, idx, (void *)&elem);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to replace", error);
            simplebuffer_free(elem);
            return error;
        }
        break;

    case INI_COMMENT_MODE_REMOVE:

        TRACE_INFO_STRING("Remove mode", "");
        error = ref_array_remove(ic->ra, idx);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to remove", error);
            return error;
        }

        break;

    case INI_COMMENT_MODE_CLEAR:

        TRACE_INFO_STRING("Clear mode", "");
        error = ref_array_replace(ic->ra, idx, (void *)&elem);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to replace", error);
            simplebuffer_free(elem);
            return error;
        }
        break;

    default :

        TRACE_ERROR_STRING("Coding error", "");
        simplebuffer_free(elem);
        return EINVAL;

    }


    /* Change state */
    if (INI_COMMENT_MODE_BUILD) ic->state = INI_COMMENT_READ;
    else ic->state = INI_COMMENT_CHANGED;


    TRACE_FLOW_EXIT();
    return error;
}

/*
 * Build up a comment object - use this when reading
 * comments from a file.
 */
int ini_comment_build(struct ini_comment *ic, const char *line)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_comment_modify(ic, INI_COMMENT_MODE_BUILD, 0, line, 0);

    TRACE_FLOW_NUMBER("ini_comment_build - Returning", error);
    return error;
}

/*
 * Build up a comment object - use this when reading
 * comments from a file.
 */
int ini_comment_build_wl(struct ini_comment *ic,
                         const char *line,
                         uint32_t length)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_comment_modify(ic, INI_COMMENT_MODE_BUILD, 0, line, length);

    TRACE_FLOW_NUMBER("ini_comment_build - Returning", error);
    return error;
}

/*
 * Modify comment by instering a line.
 */
int ini_comment_insert(struct ini_comment *ic,
                       uint32_t idx,
                       const char *line)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_comment_modify(ic, INI_COMMENT_MODE_INSERT, idx, line, 0);

    TRACE_FLOW_NUMBER("ini_comment_insert - Returning", error);
    return error;
}

/* Modify comment by appending a line. */
int ini_comment_append(struct ini_comment *ic, const char *line)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_comment_modify(ic, INI_COMMENT_MODE_APPEND, 0, line, 0);

    TRACE_FLOW_NUMBER("ini_comment_append - Returning", error);
    return error;
}

/* Remove line from the comment.*/
int ini_comment_remove(struct ini_comment *ic, uint32_t idx)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_comment_modify(ic, INI_COMMENT_MODE_REMOVE, idx, NULL, 0);

    TRACE_FLOW_NUMBER("ini_comment_remove - Returning", error);
    return error;
}

/* Clear line in the comment. Line is replaced with an empty line */
int ini_comment_clear(struct ini_comment *ic, uint32_t idx)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_comment_modify(ic, INI_COMMENT_MODE_CLEAR, idx, NULL, 0);

    TRACE_FLOW_NUMBER("ini_comment_clear - Returning", error);
    return error;

}

/* Replace a line in the comment */
int ini_comment_replace(struct ini_comment *ic,
                        uint32_t idx,
                        const char *line)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    error = ini_comment_modify(ic, INI_COMMENT_MODE_REPLACE, idx, line, 0);

    TRACE_FLOW_NUMBER("ini_comment_replace - Returning", error);
    return error;
}


/* Reset the comment - clean all lines.*/
int ini_comment_reset(struct ini_comment *ic)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if (!ic) {
        TRACE_ERROR_NUMBER("Invalid comment object", EINVAL);
        return EINVAL;
    }

    /* Reset comment if it is not empty */
    if (ic->state != INI_COMMENT_EMPTY) {
        ref_array_reset(ic->ra);
        ic->state = INI_COMMENT_CHANGED;
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Get number of lines */
int ini_comment_get_numlines(struct ini_comment *ic, uint32_t *num)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if ((!ic) || (!num)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    error = ref_array_getlen(ic->ra, num);

    TRACE_FLOW_NUMBER("ini_comment_get_numlines - Returning", error);
    return error;

}

/* Get line */
int ini_comment_get_line(struct ini_comment *ic, uint32_t idx,
                         char **line, uint32_t *line_len)
{
    int error = EOK;
    void *res = NULL;
    struct simplebuffer *sb = NULL;
    const unsigned char *ln;

    TRACE_FLOW_ENTRY();

    if ((!ic) || (!line)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    res = ref_array_get(ic->ra, idx, (void *)&sb);
    if (!res) {
        error = EINVAL;
        *line = NULL;
        if (line_len) line_len = 0;
    }
    else {
        ln = simplebuffer_get_buf(sb);
        memcpy(line, &ln, sizeof(char *));
        if (line_len) *line_len = simplebuffer_get_len(sb);
    }

    TRACE_FLOW_NUMBER("ini_comment_get_line - Returning", error);
    return error;
}

/* Swap lines */
int ini_comment_swap(struct ini_comment *ic,
                     uint32_t idx1,
                     uint32_t idx2)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if (!ic) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    if (idx1 != idx2) {
        if ((error = ref_array_swap(ic->ra, idx1, idx2))) {
            TRACE_ERROR_NUMBER("Failed to swap", error);
            return error;
        }
        ic->state = INI_COMMENT_CHANGED;
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Add one comment to another */
int ini_comment_add(struct ini_comment *ic_to_add,
                    struct ini_comment *ic)
{
    int error = EOK;
    struct simplebuffer *sb = NULL;
    struct simplebuffer *sb_new = NULL;
    void *res = NULL;
    uint32_t len = 0;
    int i;

    TRACE_FLOW_ENTRY();

    len = ref_array_len(ic_to_add->ra);

    for (i = 0; i< len; i++) {

        /* Get data element */
        res = ref_array_get(ic_to_add->ra, i, (void *)(&sb));
        if (!res) {
            TRACE_ERROR_NUMBER("Failed to get comment element", error);
            return error;
        }

        /* Create a storage a for a copy */
        error = simplebuffer_alloc(&sb_new);
        if (error) {
            TRACE_ERROR_NUMBER("Allocate buffer for the comment", error);
            return error;
        }

        /* Copy actual data */
        error = simplebuffer_add_str(sb_new,
                                     (const char *)simplebuffer_get_buf(sb),
                                     simplebuffer_get_len(sb),
                                     INI_COMMENT_LEN);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to append line to an array", error);
            simplebuffer_free(sb_new);
            return error;
        }

        /* Append it to the array */
        error = ref_array_append(ic->ra, (void *)&sb_new);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to append element to an array", error);
            simplebuffer_free(sb_new);
            return error;
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Serialize comment */
int ini_comment_serialize (struct ini_comment *ic,
                           struct simplebuffer *sbobj)
{
    int error = EOK;
    uint32_t num = 0;
    uint32_t i = 0;
    uint32_t len = 0;
    char *commentline = NULL;

    TRACE_FLOW_ENTRY();

    /* Get number of lines in the comment */
    error = ini_comment_get_numlines(ic, &num);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to get number of lines", error);
        return error;
    }

    for (i = 0; i < num; i++) {

        len = 0;
        commentline = NULL;

        error = ini_comment_get_line(ic, i, &commentline, &len);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to get line", error);
            return error;
        }

        error = simplebuffer_add_raw(sbobj,
                                     commentline,
                                     len,
                                     INI_VALUE_BLOCK);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to add comment", error);
            return error;
        }

        error = simplebuffer_add_cr(sbobj);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to add CR", error);
            return error;
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Internal function to print comment */
void ini_comment_print(struct ini_comment *ic, FILE *file)
{
    int len;
    int i;
    struct simplebuffer *sb = NULL;

    TRACE_FLOW_ENTRY();

    if (!file) {
        TRACE_ERROR_NUMBER("Invalid file argument", EINVAL);
        return;
    }

    if (ic) {
        len = ref_array_len(ic->ra);
        for (i = 0; i < len; i++) {
            ref_array_get(ic->ra, i, (void *)(&sb));
            fprintf(file, "%s\n", simplebuffer_get_buf(sb));
        }
    }

    TRACE_FLOW_EXIT();
}
