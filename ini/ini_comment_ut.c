/*
    INI LIBRARY

    Unit test for the comment object.

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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define TRACE_HOME
#include "trace.h"
#include "ini_comment.h"

int verbose = 0;

#define INIOUT(foo) \
    do { \
        if (verbose) foo; \
    } while(0)

typedef int (*test_fn)(void);

static int file_test(void)
{
    int error = EOK;
    struct ini_comment *ic = NULL;

    if ((error = ini_comment_create(&ic)) ||
        (error = ini_comment_build(ic, ";Line 0")) ||
        (error = ini_comment_build(ic, ";Line 1")) ||
        (error = ini_comment_build(ic, ";Line 2"))) {
        printf("Failed to create comment object %d\n",
               error);
        ini_comment_destroy(ic);
        return error;
    }

    INIOUT(printf("<==== Comment ====>\n"));
    INIOUT(ini_comment_print(ic, stdout));
    INIOUT(printf("<=================>\n"));
    ini_comment_destroy(ic);

    return error;
}


static int alter_test(void)
{
    int error = EOK;
    struct ini_comment *ic = NULL;
    uint32_t i, num = 0;
    char *line = NULL;
    const char *expected[] = { ";Line 0 inserted",
                               ";Line 1 inserted",
                               "",
                               "",
                               ";Line 3 replaced",
                               "",
                               ";Line 4" };


    if ((error = ini_comment_create(&ic)) ||
        (error = ini_comment_build(ic, ";Line 0")) ||
        (error = ini_comment_build(ic, NULL)) ||
        (error = ini_comment_build(ic, ";Line 2")) ||
        (error = ini_comment_build(ic, ";Line 3")) ||
        (error = ini_comment_build(ic, ""))) {
        printf("Failed to create comment object\n");
        ini_comment_destroy(ic);
        return error;
    }

    INIOUT(printf("<==== Comment ====>\n"));
    INIOUT(ini_comment_print(ic, stdout));
    INIOUT(printf("<=================>\n"));

    if ((error = ini_comment_append(ic, ";Line 4")) ||
        (error = ini_comment_clear(ic, 2)) ||
        (error = ini_comment_replace(ic, 3, ";Line 3 replaced")) ||
        (error = ini_comment_remove(ic, 0)) ||
        (error = ini_comment_insert(ic, 0, ";Line 0 inserted")) ||
        (error = ini_comment_insert(ic, 1, ";Line 1 inserted"))) {
        printf("Failed to create comment object\n");
        ini_comment_destroy(ic);
        return error;
    }

    INIOUT(printf("<==== Comment ====>\n"));
    INIOUT(ini_comment_print(ic, stdout));
    INIOUT(printf("<=================>\n"));

    error = ini_comment_get_numlines(ic, &num);
    if (error) {
        printf("Failed to get number of lines.\n");
        ini_comment_destroy(ic);
        return error;
    }

    for (i = 0; i < num; i++) {
        error = ini_comment_get_line(ic, i, &line, NULL);
        if (error) {
            printf("Failed to get line.\n");
            ini_comment_destroy(ic);
            return error;
        }
        if (strcmp(line, expected[i]) != 0) {
            printf("Lines do not match.\n");
            printf("GOT: %s\n", line);
            printf("EXP: %s\n", expected[i]);
            ini_comment_destroy(ic);
            return error;
        }
    }

    INIOUT(printf("\n\nSwap test\n\n"));

    if ((error = ini_comment_swap(ic, 0 , 6)) ||
        (error = ini_comment_swap(ic, 1 , 5)) ||
        (error = ini_comment_swap(ic, 2 , 4))) {
        printf("Failed to swap lines.\n");
        ini_comment_destroy(ic);
        return error;
    }

    for (i = 0; i < num; i++) {
        error = ini_comment_get_line(ic, i, &line, NULL);
        if (error) {
            printf("Failed to get line.\n");
            ini_comment_destroy(ic);
            return error;
        }
        if (strcmp(line, expected[6 - i]) != 0) {
            printf("Lines do not match.\n");
            printf("GOT: %s\n", line);
            printf("EXP: %s\n", expected[6 - i]);
            ini_comment_destroy(ic);
            return error;
        }
    }

    ini_comment_destroy(ic);
    return error;
}

static int copy_test(void)
{
    int error = EOK;
    struct ini_comment *ic = NULL;
    struct ini_comment *ic_copy = NULL;
    char *line = NULL;
    char *line_copy = NULL;
    uint32_t i, num = 0;

    INIOUT(printf("\n\nCopy test\n\n"));

    if ((error = ini_comment_create(&ic)) ||
        (error = ini_comment_build(ic, ";Line 0")) ||
        (error = ini_comment_build(ic, ";Line 1")) ||
        (error = ini_comment_build(ic, ";Line 2"))) {
        printf("Failed to create comment object %d\n",
               error);
        ini_comment_destroy(ic);
        return error;
    }

    INIOUT(printf("<==== Comment ====>\n"));
    INIOUT(ini_comment_print(ic, stdout));
    INIOUT(printf("<=================>\n"));

    if ((error = ini_comment_copy(ic, &ic_copy))) {
        printf("Failed to create comment object %d\n",
               error);
        ini_comment_destroy(ic);
        return error;
    }

    INIOUT(printf("<==== Comment Copy====>\n"));
    INIOUT(ini_comment_print(ic_copy, stdout));
    INIOUT(printf("<=================>\n"));

    error = ini_comment_get_numlines(ic, &num);
    if (error) {
        printf("Failed to get number of lines.\n");
        ini_comment_destroy(ic);
        ini_comment_destroy(ic_copy);
        return error;
    }

    for (i = 0; i < num; i++) {
        error = ini_comment_get_line(ic, i, &line, NULL);
        if (error) {
            printf("Failed to get line.\n");
            ini_comment_destroy(ic);
            ini_comment_destroy(ic_copy);
            return error;
        }
        error = ini_comment_get_line(ic_copy, i, &line_copy, NULL);
        if (error) {
            printf("Failed to get line.\n");
            ini_comment_destroy(ic);
            ini_comment_destroy(ic_copy);
            return error;
        }
        if (strcmp(line, line_copy) != 0) {
            printf("Lines do not match.\n");
            printf("Source: %s\n", line);
            printf("Copy: %s\n", line_copy);
            ini_comment_destroy(ic);
            ini_comment_destroy(ic_copy);
            return -1;
        }
    }

    ini_comment_destroy(ic);
    ini_comment_destroy(ic_copy);

    return error;
}

static int add_test(void)
{
    int error = EOK;
    struct ini_comment *ic = NULL;
    struct ini_comment *ic_to_add = NULL;
    struct ini_comment *ic_cmp = NULL;
    uint32_t i, num1 = 0, num2 = 0;
    char *line1 = NULL;
    char *line2 = NULL;


    INIOUT(printf("\n\nAdd test\n\n"));

    if ((error = ini_comment_create(&ic)) ||
        (error = ini_comment_build(ic, ";Line 0")) ||
        (error = ini_comment_build(ic, ";Line 1")) ||
        (error = ini_comment_build(ic, ";Line 2"))) {
        printf("Failed to create comment object %d\n",
               error);
        ini_comment_destroy(ic);
        return error;
    }

    INIOUT(printf("<==== Comment ====>\n"));
    INIOUT(ini_comment_print(ic, stdout));
    INIOUT(printf("<=================>\n"));

    if ((error = ini_comment_create(&ic_to_add)) ||
        (error = ini_comment_build(ic_to_add, ";Line 3")) ||
        (error = ini_comment_build(ic_to_add, ";Line 4")) ||
        (error = ini_comment_build(ic_to_add, ";Line 5"))) {
        printf("Failed to create comment object %d\n",
               error);
        ini_comment_destroy(ic);
        return error;
    }

    INIOUT(printf("<==== Comment To Add ====>\n"));
    INIOUT(ini_comment_print(ic_to_add, stdout));
    INIOUT(printf("<=================>\n"));

    error = ini_comment_add(ic_to_add, ic);
    if (error) {
        printf("Failed to add one comment to another.\n");
        ini_comment_destroy(ic);
        ini_comment_destroy(ic_to_add);
        return error;
    }

    INIOUT(printf("<==== Merged Comment ====>\n"));
    INIOUT(ini_comment_print(ic, stdout));
    INIOUT(printf("<=================>\n"));

    if ((error = ini_comment_create(&ic_cmp)) ||
        (error = ini_comment_build(ic_cmp, ";Line 0")) ||
        (error = ini_comment_build(ic_cmp, ";Line 1")) ||
        (error = ini_comment_build(ic_cmp, ";Line 2")) ||
        (error = ini_comment_build(ic_cmp, ";Line 3")) ||
        (error = ini_comment_build(ic_cmp, ";Line 4")) ||
        (error = ini_comment_build(ic_cmp, ";Line 5"))) {
        printf("Failed to create comment object %d\n",
               error);
        ini_comment_destroy(ic_cmp);
        return error;
    }

    ini_comment_destroy(ic_to_add);

    error = ini_comment_get_numlines(ic, &num1);
    if (error) {
        printf("Failed to get number of lines.\n");
        ini_comment_destroy(ic);
        ini_comment_destroy(ic_cmp);
        return error;
    }

    error = ini_comment_get_numlines(ic, &num2);
    if (error) {
        printf("Failed to get number of lines.\n");
        ini_comment_destroy(ic);
        ini_comment_destroy(ic_cmp);
        return error;
    }

    if (num1 != num2) {
        printf("Sizes are different.\n");
        ini_comment_destroy(ic);
        ini_comment_destroy(ic_cmp);
        return -1;
    }

    for (i = 0; i < num1; i++) {
        line1 = NULL;
        error = ini_comment_get_line(ic, i, &line1, NULL);
        if (error) {
            printf("Failed to get line.\n");
            ini_comment_destroy(ic);
            ini_comment_destroy(ic_cmp);
            return error;
        }
        line2 = NULL;
        error = ini_comment_get_line(ic_cmp, i, &line2, NULL);
        if (error) {
            printf("Failed to get line.\n");
            ini_comment_destroy(ic);
            ini_comment_destroy(ic_cmp);
            return error;
        }
        if (strcmp(line1, line2) != 0) {
            printf("Lines do not match.\n");
            printf("1st: %s\n", line1);
            printf("2nd: %s\n", line2);
            ini_comment_destroy(ic);
            ini_comment_destroy(ic_cmp);
            return -1;
        }
    }

    ini_comment_destroy(ic);
    ini_comment_destroy(ic_cmp);

    return error;
}


int main(int argc, char *argv[])
{
    int error = EOK;
    test_fn tests[] = { file_test,
                        alter_test,
                        copy_test,
                        add_test,
                        NULL };
    test_fn t;
    int i = 0;
    char *var;

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = 1;
    else {
        var = getenv("COMMON_TEST_VERBOSE");
        if (var) verbose = 1;
    }

    INIOUT(printf("Start\n"));

    while ((t = tests[i++])) {
        error = t();
        if (error) {
            INIOUT(printf("Failed with error %d!\n", error));
            return error;
        }
    }

    INIOUT(printf("Success!\n"));
    return 0;
}
