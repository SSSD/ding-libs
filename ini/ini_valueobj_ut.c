/*
    INI LIBRARY

    Unit test for the value object.

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

#define _GNU_SOURCE
#include <errno.h>  /* for errors */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "ini_valueobj.h"
#include "ini_defines.h"
#include "config.h"
#define TRACE_HOME
#include "trace.h"

int verbose = 0;

#define VOOUT(foo) \
    do { \
        if (verbose) foo; \
    } while(0)


typedef int (*test_fn)(void);


extern void ref_array_debug(struct ref_array *ra);

static int create_comment(int i, struct ini_comment **ic)
{
    int error = EOK;
    const char *template = ";Line 0 of the value %d";
    char comment[80];
    struct ini_comment *new_ic = NULL;

    TRACE_FLOW_ENTRY();

    sprintf(comment, template, i);


    if ((error = ini_comment_create(&new_ic)) ||
        (error = ini_comment_build(new_ic, comment)) ||
        (error = ini_comment_build(new_ic, NULL)) ||
        (error = ini_comment_build(new_ic, "#This is the second line")) ||
        (error = ini_comment_build(new_ic, ";This is the third line")) ||
        (error = ini_comment_build(new_ic, ""))) {
        printf("Failed to create comment object\n");
        ini_comment_destroy(new_ic);
        return -1;
    }

    *ic = new_ic;

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Save value to the file */
/* NOTE: might be moved into the API in future */
int save_value(FILE *ff, const char *key, struct value_obj *vo)
{

    int error = EOK;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;

    TRACE_FLOW_ENTRY();

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    /* Serialize */
    error = value_serialize(vo, key, sbobj);
    if (error) {
        printf("Failed to serialize a value object %d.\n", error);
        simplebuffer_free(sbobj);
        return error;
    }

    /* Add CR */
    error = simplebuffer_add_cr(sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to add CR", error);
        simplebuffer_free(sbobj);
        return error;
    }

    /* Save */
    left = simplebuffer_get_len(sbobj);
    while (left > 0) {
        error = simplebuffer_write(fileno(ff), sbobj, &left);
        if (error) {
            printf("Failed to write value object %d.\n", error);
            simplebuffer_free(sbobj);
            return error;
        }
    }

    simplebuffer_free(sbobj);

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Test to create value object using arrays */
int other_create_test(FILE *ff, struct value_obj **vo)
{
    int error = EOK;
    struct value_obj *new_vo = NULL;
    struct ref_array *raw_lines;
    struct ref_array *raw_lengths;
    struct ini_comment *ic = NULL;
    struct ini_comment *ic2 = NULL;
    char *val;
    const char *vallines[] = { "Domain1,",
                               "  Domain2 ,",
                               "  Domain3" };
    const char *fullstr;
    const char *expected = "Domain1,  Domain2 ,  Domain3";
    int i;
    uint32_t origin = 0;
    uint32_t line = 0;


    TRACE_FLOW_ENTRY();

    /* Create a pair of arrays */
    error = value_create_arrays(&raw_lines,
                                &raw_lengths);
    if (error) {
        printf("Failed to create arrays %d.\n", error);
        return error;
    }

    for (i=0; i< 3; i++) {
        errno = 0;
        val = strdup(vallines[i]);
        if (val == NULL) {
            error = errno;
            printf("Failed to dup memory %d.\n", error);
            value_destroy_arrays(raw_lines,
                                raw_lengths);
            return error;
        }

        /* Add line to the arrays */
        error = value_add_to_arrays(val,
                                    strlen(val),
                                    raw_lines,
                                    raw_lengths);
        if (error) {
            printf("Failed to add to arrays %d.\n", error);
            value_destroy_arrays(raw_lines,
                                raw_lengths);
            return error;
        }

    }

    /* Create a comment */
    error = create_comment(1000, &ic);
    if (error) {
        printf("Failed to create comment %d.\n", error);
        value_destroy_arrays(raw_lines,
                            raw_lengths);
        return error;
    }

    /* Create object */
    error = value_create_from_refarray(raw_lines,
                                       raw_lengths,
                                       1,
                                       INI_VALUE_READ,
                                       3,
                                       70,
                                       ic,
                                       &new_vo);

    if (error) {
        printf("Failed to create comment %d.\n", error);
        value_destroy_arrays(raw_lines,
                            raw_lengths);
        ini_comment_destroy(ic);
        return error;
    }

    /* Save value to the file */
    error = save_value(ff, "baz", new_vo);
    if (error) {
        printf("Failed to save value to file %d.\n", error);
        value_destroy(new_vo);
        return error;
    }

    /* Now do assertions and modifications to the object */

    /* NOTE: Below this line do not need to free arrays or comment
     * they became internal parts of the value object
     * and will be freed as a part of it.
     */

    /* Get concatenated value */
    error = value_get_concatenated(new_vo,
                                   &fullstr);

    if (error) {
        printf("Failed to get the string %d.\n", error);
        value_destroy(new_vo);
        return error;
    }

    if (strncmp(fullstr, expected, strlen(expected) + 1) != 0) {
        printf("The expected value is different.\n%s\n", fullstr);
        value_destroy(new_vo);
        return error;
    }

    /* Get value's origin */
    error = value_get_origin(new_vo, &origin);
    if (error) {
        printf("Failed to get origin %d.\n", error);
        value_destroy(new_vo);
        return error;
    }

    if (origin != INI_VALUE_READ) {
        printf("The expected origin is different.\n%d\n", origin);
        value_destroy(new_vo);
        return error;
    }

    /* Get value's line */
    error = value_get_line(new_vo, &line);
    if (error) {
        printf("Failed to get origin %d.\n", error);
        value_destroy(new_vo);
        return error;
    }

    if (line != 1) {
        printf("The expected line is different.\n%d\n", origin);
        value_destroy(new_vo);
        return error;
    }

    /* Get comment from the value */
    ic = NULL;
    error = value_extract_comment(new_vo, &ic);
    if (error) {
        printf("Failed to extract comment %d.\n", error);
        value_destroy(new_vo);
        return error;
    }

    if (ic == NULL) {
        printf("The expected comment to be there.\n");
        value_destroy(new_vo);
        return error;
    }

    VOOUT(ini_comment_print(ic, stdout));

    /* Get comment again */
    ic2 = NULL;
    error = value_extract_comment(new_vo, &ic2);
    if (error) {
        printf("Failed to extract comment %d.\n", error);
        value_destroy(new_vo);
        ini_comment_destroy(ic);
        return error;
    }

    if (ic2 != NULL) {
        printf("The expected NO comment to be there.\n");
        value_destroy(new_vo);
        ini_comment_destroy(ic);
        /* No free for ic2 since it is the same object */

        /* But this should not happen anyways -
         * it will be coding error.
         */
        return error;
    }

    /* Put comment back */
    error = value_put_comment(new_vo, ic);
    if (error) {
        printf("Failed to put comment back %d.\n", error);
        value_destroy(new_vo);
        ini_comment_destroy(ic);
        return error;
    }

    /* Save value to the file */
    error = save_value(ff, "bar", new_vo);
    if (error) {
        printf("Failed to save value to file %d.\n", error);
        value_destroy(new_vo);
        return error;
    }

    *vo = new_vo;

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Modify the value object */
int modify_test(FILE *ff, struct value_obj *vo)
{
    int error = EOK;
    const char *strval = "Domain100, Domain200, Domain300";

    TRACE_FLOW_ENTRY();


    /* Update key length */
    error = value_set_keylen(vo, strlen("foobar"));
    if (error) {
        printf("Failed to change key length %d.\n", error);
        return error;
    }

    /* Update value */
    error = value_update(vo,
                         strval,
                         strlen(strval),
                         INI_VALUE_CREATED,
                         10);
    if (error) {
        printf("Failed to update value %d.\n", error);
        return error;
    }


    /* Save value to the file */
    error = save_value(ff, "foobar", vo);
    if (error) {
        printf("Failed to save value to file %d.\n", error);
        return error;
    }

    TRACE_FLOW_EXIT();
    return EOK;
}


int vo_basic_test(void)
{
    int error = EOK;
    const char *strvalue = "Test multi_word_value_that_will_"
                           "be_split_between_several_lines_!";

    /* Other testing can be done with the following string:
     * const char *strvalue = "Test multi word value that "
     *                        "will be split between several lines";
     */

    struct value_obj *vo = NULL;
    uint32_t wrap = 0;
    struct ini_comment *ic = NULL;
    FILE *ff = NULL;

    TRACE_FLOW_ENTRY();

    errno = 0;
    ff = fopen("test.ini","wt");
    if (ff == NULL) {
        error = errno;
        printf("Failed to open file. Error %d.\n", error);
        return error;
    }


    for (wrap = 0; wrap < 80; wrap++) {

        ic = NULL;
        error = create_comment(wrap, &ic);
        if (error) {
            printf("Failed to create a new value object %d.\n", error);
            fclose(ff);
            return error;
        }

        error = value_create_new(strvalue,
                                 strlen(strvalue),
                                 INI_VALUE_CREATED,
                                 3,
                                 wrap,
                                 ic,
                                 &vo);
        if (error) {
            printf("Failed to create a new value object %d.\n", error);
            ini_comment_destroy(ic);
            fclose(ff);
            return error;
        }

        error = save_value(ff, "key", vo);
        if (error) {
            printf("Failed to save value to file %d.\n", error);
            value_destroy(vo);
            fclose(ff);
            return error;
        }

        value_destroy(vo);
    }

    /* Run other create test here */
    error = other_create_test(ff, &vo);
    if (error) {
        printf("Create test failed %d.\n", error);
        fclose(ff);
        return error;
    }

    /* Run modify test here */
    error = modify_test(ff, vo);
    if (error) {
        printf("Modify test failed %d.\n", error);
        fclose(ff);
        value_destroy(vo);
        return error;
    }

    value_destroy(vo);


    ic = NULL;
    error = create_comment(100, &ic);
    if (error) {
        printf("Failed to create a new value object %d.\n", error);
        fclose(ff);
        return error;
    }

    ini_comment_print(ic, ff);

    ini_comment_destroy(ic);

    fclose(ff);

    TRACE_FLOW_EXIT();
    return EOK;
}

int vo_copy_test(void)
{
    int error = EOK;
    const char *strvalue = "Test multi word value that "
                           "will be split between several lines";

    struct value_obj *vo = NULL;
    struct value_obj *vo_copy = NULL;
    uint32_t wrap = 0;
    struct ini_comment *ic = NULL;
    FILE *ff = NULL;
    char comment[100];

    TRACE_FLOW_ENTRY();

    VOOUT(printf("Copy test\n"));

    errno = 0;
    ff = fopen("test.ini","a");
    if (ff == NULL) {
        error = errno;
        printf("Failed to open file. Error %d.\n", error);
        return error;
    }

    error = ini_comment_create(&ic);
    if (error) {
        printf("Failed to create comment object\n");
        fclose(ff);
        return -1;
    }

    error = ini_comment_append(ic, "#This is a copy test!");
    if (error) {
        printf("Failed to add a line to the comment %d.\n", error);
        ini_comment_destroy(ic);
        fclose(ff);
        return error;
    }

    error = ini_comment_append(ic, "#Replacable comment line");
    if (error) {
        printf("Failed to add a line to the comment %d.\n", error);
        ini_comment_destroy(ic);
        fclose(ff);
        return error;
    }

    error = value_create_new(strvalue,
                             strlen(strvalue),
                             INI_VALUE_CREATED,
                             3,
                             20,
                             ic,
                             &vo);
    if (error) {
        printf("Failed to create a new value object %d.\n", error);
        ini_comment_destroy(ic);
        fclose(ff);
        return error;
    }

    error = save_value(ff, "key", vo);
    if (error) {
        printf("Failed to save value to file %d.\n", error);
        value_destroy(vo);
        return error;
    }

    for (wrap = 0; wrap < 80; wrap++) {

        TRACE_INFO_NUMBER("Iteration:", wrap);

        error = value_copy(vo, &vo_copy);
        if (error) {
            printf("Failed to create a new value object %d.\n", error);
            value_destroy(vo);
            fclose(ff);
            return error;
        }

        error = value_set_boundary(vo_copy, wrap);
        if (error) {
            printf("Failed to set boundary %d.\n", error);
            value_destroy(vo);
            value_destroy(vo_copy);
            fclose(ff);
            return error;
        }

        /* Get comment from the value */
        error = value_extract_comment(vo_copy, &ic);
        if (error) {
            printf("Failed to extract comment %d.\n", error);
            value_destroy(vo);
            value_destroy(vo_copy);
            fclose(ff);
            return error;
        }

        /* Replace comment in the value */
        sprintf(comment, ";This is value with boundary %d", wrap);
        VOOUT(printf("Comment: %s\n", comment));
        error = ini_comment_replace(ic, 1, comment);
        if (error) {
            printf("Failed to replace comment %d.\n", error);
            value_destroy(vo);
            value_destroy(vo_copy);
            fclose(ff);
            return error;
        }

        /* Set comment into the value */
        error = value_put_comment(vo_copy, ic);
        if (error) {
            printf("Failed to set comment %d.\n", error);
            value_destroy(vo);
            value_destroy(vo_copy);
            fclose(ff);
            return error;
        }

        error = save_value(ff, "key", vo_copy);
        if (error) {
            printf("Failed to save value to file %d.\n", error);
            value_destroy(vo);
            value_destroy(vo_copy);
            fclose(ff);
            return error;
        }

        value_destroy(vo_copy);
    }

    value_destroy(vo);
    TRACE_FLOW_EXIT();
    return EOK;
}

int vo_show_test(void)
{
    VOOUT(system("cat test.ini"));
    return EOK;
}

/* Main function of the unit test */
int main(int argc, char *argv[])
{
    int error = 0;
    test_fn tests[] = { vo_basic_test,
                        vo_copy_test,
                        vo_show_test,
                        NULL };
    test_fn t;
    int i = 0;
    char *var;

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = 1;
    else {
        var = getenv("COMMON_TEST_VERBOSE");
        if (var) verbose = 1;
    }

    VOOUT(printf("Start\n"));

    while ((t = tests[i++])) {
        error = t();
        if (error) {
            VOOUT(printf("Failed with error %d!\n", error));
            return error;
        }
    }

    VOOUT(printf("Success!\n"));
    return 0;
}
