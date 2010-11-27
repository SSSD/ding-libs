/*
    INI LIBRARY

    Unit test for the parser object.

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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include "ini_defines.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "simplebuffer.h"
#include "path_utils.h"
#include "config.h"
#define TRACE_HOME
#include "trace.h"
#include "collection_tools.h"

int verbose = 0;
char *confdir = NULL;

#define INIOUT(foo) \
    do { \
        if (verbose) foo; \
    } while(0)

typedef int (*test_fn)(void);

int test_one_file(const char *in_filename,
                  const char *out_filename)
{
    int error = EOK;
    struct ini_cfgfile *file_ctx = NULL;
    FILE *ff = NULL;
    struct ini_cfgobj *ini_config = NULL;
    struct ini_cfgobj *ini_copy = NULL;
    char **error_list = NULL;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;

    INIOUT(printf("<==== Testing file %s ====>\n", in_filename));

    /* Create config collection */
    error = ini_config_create(&ini_config);
    if (error) {
        printf("Failed to create collection. Error %d.\n", error);
        return error;
    }

    error = ini_config_file_open(in_filename,
                                 INI_STOP_ON_NONE,
                                 0, /* TBD */
                                 0, /* TBD */
                                 &file_ctx);
    if (error) {
        printf("Failed to open file for reading. Error %d.\n",  error);
        ini_config_destroy(ini_config);
        return error;
    }

    error = ini_config_parse(file_ctx,
                             ini_config);
    if (error) {
        INIOUT(printf("Failed to parse configuration. Error %d.\n", error));

        if (ini_config_error_count(file_ctx)) {
            INIOUT(printf("Errors detected while parsing: %s\n",
                   ini_config_get_filename(file_ctx)));
            ini_config_get_errors(file_ctx, &error_list);
            INIOUT(ini_print_errors(stdout, error_list));
            ini_config_free_errors(error_list);
        }
        /* We do not return here intentionally */
    }

    ini_config_file_close(file_ctx);

    INIOUT(col_debug_collection(ini_config->cfg, COL_TRAVERSE_DEFAULT));

    /* Copy configuration */
    error = ini_config_copy(ini_config, &ini_copy);
    if (error) {
        printf("Failed to copy configuration. Error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    ini_config_destroy(ini_config);
    ini_config = ini_copy;

    INIOUT(col_debug_collection(ini_config->cfg, COL_TRAVERSE_DEFAULT));

    error = ini_config_set_wrap(ini_config, 5);
    if (error) {
        printf("Failed to set custom wrapper. Error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        ini_config_destroy(ini_config);
        return error;
    }

    error = ini_config_serialize(ini_config, sbobj);
    if (error != EOK) {
        printf("Failed to serialize configuration. Error %d.\n", error);
        ini_config_destroy(ini_config);
        simplebuffer_free(sbobj);
        return error;
    }

    errno = 0;
    ff = fopen(out_filename, "w");
    if(!ff) {
        error = errno;
        printf("Failed to open file for writing. Error %d.\n", error);
        ini_config_destroy(ini_config);
        simplebuffer_free(sbobj);
        return error;
    }

    /* Save */
    left = simplebuffer_get_len(sbobj);
    while (left > 0) {
        error = simplebuffer_write(fileno(ff), sbobj, &left);
        if (error) {
            printf("Failed to write back the configuration %d.\n", error);
            simplebuffer_free(sbobj);
            ini_config_destroy(ini_config);
            fclose(ff);
            return error;
        }
    }

    ini_config_destroy(ini_config);
    simplebuffer_free(sbobj);
    fclose(ff);

    return EOK;
}


/* Run tests for multiple files */
int read_save_test(void)
{
    int error = EOK;
    int i = 0;
    char infile[PATH_MAX];
    char outfile[PATH_MAX];
    char *srcdir;
    const char *files[] = { "real",
                            "mysssd",
                            "ipa",
                            "test",
                            NULL };


    srcdir = getenv("srcdir");

    while(files[i]) {

        sprintf(infile, "%s/ini/ini.d/%s.conf", (srcdir == NULL) ? "." : srcdir,
                                                files[i]);
        sprintf(outfile, "%s/%s.conf.out", (srcdir == NULL) ? "." : srcdir,
                                           files[i]);
        error = test_one_file(infile, outfile);
        INIOUT(printf("Test for file: %s returned %d\n", files[i], error));
        i++;
    }

    return EOK;
}

/* Run tests for multiple files */
int read_again_test(void)
{
    int error = EOK;
    int i = 0;
    char infile[PATH_MAX];
    char outfile[PATH_MAX];
    char *srcdir;
    char command[PATH_MAX * 3];
    const char *files[] = { "real",
                            "mysssd",
                            "ipa",
                            "test",
                            NULL };


    srcdir = getenv("srcdir");

    while(files[i]) {

        sprintf(infile, "%s/%s.conf.out", (srcdir == NULL) ? "." : srcdir,
                                          files[i]);
        sprintf(outfile, "%s/%s.conf.2.out", (srcdir == NULL) ? "." : srcdir,
                                             files[i]);
        error = test_one_file(infile, outfile);
        INIOUT(printf("Test for file: %s returned %d\n", files[i], error));
        if (error) break;
        sprintf(command,"diff -q %s %s", infile, outfile);
        error = system(command);
        INIOUT(printf("Comparison of %s %s returned: %d\n",
                      infile, outfile, error));
        if (error) break;

        i++;
    }

    return error;
}


/* Main function of the unit test */
int main(int argc, char *argv[])
{
    int error = 0;
    test_fn tests[] = { read_save_test,
                        read_again_test,
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
