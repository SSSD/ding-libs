/*
    Simple buffer

    Basic buffer manipulation routines. Taken from ELAPI code.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2009 - 2010

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include <errno.h>      /* for errors */
#include <stdlib.h>     /* for free() */
#include <unistd.h>     /* for write() */
#include <string.h>     /* for memcpy() */

#include "simplebuffer.h"
#include "trace.h"

/* End line string */
#define ENDLNSTR "\n"

/* Function to free buffer */
void simplebuffer_free(struct simplebuffer *data)
{
    TRACE_FLOW_ENTRY();

    if (data) {
        free(data->buffer);
        free(data);
    }

    TRACE_FLOW_EXIT();
}

/* Allocate buffer structure */
int simplebuffer_alloc(struct simplebuffer **data)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if (!data) {
        TRACE_ERROR_STRING("Invalid argument", "");
        error = EINVAL;
    }
    else {
        *data = (struct simplebuffer *)calloc(1,
                                              sizeof(struct simplebuffer));
        if (*data == NULL) {
            TRACE_ERROR_STRING("Failed to allocate memory", "");
            error = ENOMEM;
        }
        else error = EOK;
    }

    TRACE_FLOW_RETURN(error);
    return error;
}


/* Grow buffer */
int simplebuffer_grow(struct simplebuffer *data,
                      uint32_t len,
                      uint32_t block)
{
    int error = EOK;
    unsigned char *newbuf = NULL;

    TRACE_FLOW_ENTRY();

    TRACE_INFO_NUMBER("Current length: ", data->length);
    TRACE_INFO_NUMBER("Current size: ", data->size);
    TRACE_INFO_NUMBER("Length to have: ", len);
    TRACE_INFO_NUMBER("Increment length: ", block);

    /* Grow buffer if needed */
    while (data->length + len >= data->size) {
        newbuf = realloc(data->buffer, data->size + block);
        if (newbuf == NULL) {
            TRACE_ERROR_NUMBER("Error. Failed to allocate memory.", ENOMEM);
            return ENOMEM;
        }
        data->buffer = newbuf;
        data->size += block;
        TRACE_INFO_NUMBER("New size: ", data->size);
    }

    TRACE_INFO_NUMBER("Final size: ", data->size);
    TRACE_FLOW_RETURN(error);
    return error;
}

/* Function to add raw data to the end of the buffer.
 * Terminating 0 is not counted in length but appended
 * automatically.
 */
int simplebuffer_add_raw(struct simplebuffer *data,
                         void *data_in,
                         uint32_t len,
                         uint32_t block)
{

    int error = EOK;
    uint32_t size;

    TRACE_FLOW_ENTRY();

    size = len + 1;
    error = simplebuffer_grow(data, size,
                             ((block > size) ? block : size));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to grow buffer.", error);
        return error;
    }

    memcpy(data->buffer + data->length, data_in, len);
    data->length += len;
    data->buffer[data->length] = '\0';

    TRACE_FLOW_EXIT();
    return error;
}

/* Function to add string to the end of the buffer. */
int simplebuffer_add_str(struct simplebuffer *data,
                         const char *str,
                         uint32_t len,
                         uint32_t block)
{

    int error = EOK;
    uint32_t size;

    TRACE_FLOW_ENTRY();

    size = len + 1;
    error = simplebuffer_grow(data, size,
                             ((block > size) ? block : size));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to grow buffer.", error);
        return error;
    }

    memcpy(data->buffer + data->length, str, len);
    data->length += len;
    data->buffer[data->length] = '\0';

    TRACE_FLOW_EXIT();
    return error;
}

/* Finction to add CR to the buffer */
int simplebuffer_add_cr(struct simplebuffer *data)
{
    int error = EOK;
    char cr[] = ENDLNSTR;

    TRACE_FLOW_ENTRY();

    error = simplebuffer_add_raw(data,
                                 (void *)cr,
                                 sizeof(ENDLNSTR) - 1,
                                 sizeof(ENDLNSTR));

    TRACE_FLOW_RETURN(error);
    return error;
}


/* Function to write data synchroniusly */
int simplebuffer_write(int fd, struct simplebuffer *data, uint32_t *left)
{
    size_t res;
    int error;

    TRACE_FLOW_ENTRY();

    /* Write given number of bytes to the socket */
    res = write(fd,
                data->buffer + (data->length - *left),
                (size_t)(*left));

    if (res == -1) {
        error = errno;
        TRACE_ERROR_NUMBER("Write failed.", error);
        return error;
    }

    (*left) -= res;

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Get buffer */
const unsigned char *simplebuffer_get_buf(struct simplebuffer *data)
{
    return data->buffer;
}

/* Get void buffer */
void *simplebuffer_get_vbuf(struct simplebuffer *data)
{
    return (void *)data->buffer;
}


/* Get length */
uint32_t simplebuffer_get_len(struct simplebuffer *data)
{
    return data->length;
}
