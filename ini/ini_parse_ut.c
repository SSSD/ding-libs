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
#include "ini_defines.h"
#include "ini_parse.h"
#include "ini_config.h"
#include "ini_configobj.h"
#include "simplebuffer.h"
#include "path_utils.h"
#include "config.h"
#define TRACE_HOME
#include "trace.h"

int verbose = 0;
char *confdir = NULL;

#define INIOUT(foo) \
    do { \
        if (verbose) foo; \
    } while(0)

typedef int (*test_fn)(void);

int test_one_file(const char *filename)
{
    int error = EOK;
    FILE *ff = NULL;
    char new_file[100];
    struct configobj *ini_config = NULL;
    struct collection_item *error_list = NULL;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;
    char filename_base[96];

    INIOUT(printf("<==== Testing file %s ====>\n", filename));

    /* Create config collection */
    error = ini_config_create(&ini_config);
    if (error != EOK) {
        printf("Failed to create collection. Error %d.\n", error);
        return error;
    }

    errno = 0;
    ff = fopen(filename,"r");
    if(!ff) {
        error = errno;
        printf("Failed to open file for reading. Error %d.\n",  error);
        ini_config_destroy(ini_config);
        return error;
    }

    error = ini_parse_config(ff,
                             filename,
                             ini_config,
                             INI_STOP_ON_NONE,
                             &error_list,
                             80);
    fclose(ff);
    if (error != EOK) {
        INIOUT(printf("Failed to parse configuration. Error %d.\n", error));
        INIOUT(print_file_parsing_errors(stdout, error_list));
        col_destroy_collection(error_list);
    }

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        ini_config_destroy(ini_config);
        return error;
    }

    error = ini_serialize_config(ini_config, sbobj);
    if (error != EOK) {
        printf("Failed to parse configuration. Error %d.\n", error);
        ini_config_destroy(ini_config);
        simplebuffer_free(sbobj);
        return error;
    }

    error = get_basename(filename_base, 96, filename);
    sprintf(new_file, "%s.out", filename_base);

    errno = 0;
    ff = fopen(new_file, "w");
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
    int lasterr = EOK;
    char *files[5];

    files[0] = malloc(sizeof(char)*512);
    sprintf(files[0], "%s/ini.d/real.conf", confdir);
    files[1] = malloc(sizeof(char)*512);
    sprintf(files[1], "%s/ini.d/mysssd.conf", confdir);
    files[2] = malloc(sizeof(char)*512);
    sprintf(files[2], "%s/ini.d/ipa.conf", confdir);
    files[3] = malloc(sizeof(char)*512);
    sprintf(files[3], "%s/ini.d/test.conf", confdir);
    files[4] = NULL;

    while(files[i]) {
        error = test_one_file(files[i]);
        INIOUT(printf("Test fo file: %s returned %d\n", files[i], error));
        if (error) lasterr = error;
        i++;
    }

    free(files[3]);
    free(files[2]);
    free(files[1]);
    free(files[0]);

    return lasterr;
}

/* Main function of the unit test */
int main(int argc, char *argv[])
{
    int error = 0;
    test_fn tests[] = { read_save_test,
                        NULL };
    char *srcdir;
    test_fn t;
    int i = 0;
    char *var;

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = 1;
    else {
        var = getenv("COMMON_TEST_VERBOSE");
        if (var) verbose = 1;
    }

    INIOUT(printf("Start\n"));

    srcdir = getenv("srcdir");
    if(!srcdir) {
        confdir = malloc(sizeof(char)*3);
        sprintf(confdir, "./ini");
    } else {
        confdir = malloc(strlen(srcdir)+4*sizeof(char));
        sprintf(confdir, "%s/ini", srcdir);
    }

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
