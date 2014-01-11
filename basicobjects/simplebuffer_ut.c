/*
    Simple buffer UNIT test

    Basic buffer manipulation routines.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2010

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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#define TRACE_HOME
#include "trace.h"
#include "simplebuffer.h"


int verbose = 0;

#define BOOUT(foo) \
    do { \
        if (verbose) foo; \
    } while(0)


static int simple_test(void)
{
    int error = EOK;
    struct simplebuffer *data = NULL;
    char str1[] = "test string 1";
    char str2[] = "test string 2";
    const char str3[] = "test string 3";
    uint32_t left = 0;
    int i;
    const unsigned char *buf;

    BOOUT(printf("Simple test start.\n"));

    error = simplebuffer_alloc(&data);
    if (error) {
        printf("Failed to allocate object %d\n", error);
        return error;
    }

    error = simplebuffer_add_raw(data,
                                 (void *)str1,
                                 strlen(str1),
                                 1);
    if (error) {
        printf("Failed to add string to an object %d\n", error);
        simplebuffer_free(data);
        return error;
    }

    error = simplebuffer_add_cr(data);
    if (error) {
        printf("Failed to add CR to an object %d\n", error);
        simplebuffer_free(data);
        return error;
    }

    error = simplebuffer_add_raw(data,
                                 (void *)str2,
                                 strlen(str2),
                                 1);
    if (error) {
        printf("Failed to add string to an object %d\n", error);
        simplebuffer_free(data);
        return error;
    }

    error = simplebuffer_add_cr(data);
    if (error) {
        printf("Failed to add CR to an object %d\n", error);
        simplebuffer_free(data);
        return error;
    }

    error = simplebuffer_add_str(data,
                                 str3,
                                 strlen(str3),
                                 1);
    if (error) {
        printf("Failed to add string to an object %d\n", error);
        simplebuffer_free(data);
        return error;
    }

	left = simplebuffer_get_len(data);
    buf = simplebuffer_get_buf(data);

    BOOUT(for(i = 0; i < left; i++) {
        printf("%02d: %02X\n", i, buf[i]);
    });

    if (verbose) {
        while (left > 0) {
            error = simplebuffer_write(1, data, &left);
            if (error) {
                printf("Failed to write to output %d\n", error);
                simplebuffer_free(data);
                return error;
            }
        }
    }

    BOOUT(printf("\n[%s]\n", simplebuffer_get_buf(data)));
    BOOUT(printf("Length: %d\n", simplebuffer_get_len(data)));


    simplebuffer_free(data);

    BOOUT(printf("Simple test end.\n"));
    return error;
}

int main(int argc, char *argv[])
{
    int error = EOK;

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = 1;

    BOOUT(printf("Start\n"));

    if ((error = simple_test())) {
        printf("Test failed! Error %d.\n", error);
        return -1;
    }

    BOOUT(printf("Success!\n"));
    return 0;
}
