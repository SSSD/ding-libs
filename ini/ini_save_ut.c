/*
    INI LIBRARY

    Unit test for the configuration object modification API.

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

#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
/* #define TRACE_LEVEL 7 */
#define TRACE_HOME
#include "trace.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "collection_tools.h"
#include "path_utils.h"

int verbose = 0;

#define WRAP_SIZE 80

#define INIOUT(foo) \
    do { \
        if (verbose) { printf("%30s(%4d): ",__FUNCTION__,__LINE__); foo; } \
    } while(0)

typedef int (*test_fn)(void);

/* Basic test */
static int basic_test(void)
{
    int error = EOK;
    char srcname[PATH_MAX];
    char resname[PATH_MAX];
    char cmpname[PATH_MAX];
    char command[PATH_MAX * 3];
    struct ini_cfgfile *file_ctx = NULL;
    char baktpl[] = "test_real_%d.conf.save";
    char *builddir = NULL;
    char *srcdir = NULL;
    struct ini_cfgobj *ini_config = NULL;
    char **error_list = NULL;
    int i;
    struct access_check acc = { INI_ACCESS_CHECK_MODE, /* Use only mode */
                                0, /* Ignore uid */
                                0, /* Ignore gid */
                                0770,
                                0 }; /* Mask is ignored */
    struct access_check new_access =
                                { INI_ACCESS_CHECK_MODE, /* Use only mode */
                                  0, /* Ignore uid */
                                  0, /* Ignore gid */
                                  0660,
                                  0 }; /* Mask is ignored */


    const char *cmp_files[] = { "real16be.conf",
                                "real16le.conf",
                                "real32le.conf",
                                "real32be.conf" };
    enum index_utf_t bom;
    enum index_utf_t bom_ar[] = { INDEX_UTF16BE,
                                  INDEX_UTF16LE,
                                  INDEX_UTF32LE,
                                  INDEX_UTF32BE };

    INIOUT(printf("<==== Start of basic test ====>\n"));

    srcdir = getenv("srcdir");
    builddir = getenv("builddir");

    snprintf(srcname, PATH_MAX, "%s/ini/ini2.d/real8.conf",
                    (srcdir == NULL) ? "." : srcdir);

    /* Create config collection */
    error = ini_config_create(&ini_config);
    if (error) {
        printf("Failed to create configuration. Error %d.\n", error);
        return error;
    }

    error = ini_config_file_open(srcname,
                                 INI_META_STATS,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file %s for reading. Error %d.\n",
                srcname, error);
        ini_config_destroy(ini_config);
        return error;
    }

    error = ini_config_parse(file_ctx,
                             INI_STOP_ON_NONE,
                             0,
                             0,
                             ini_config);
    if (error) {
        INIOUT(printf("Failed to parse configuration. Error %d.\n", error));

        if (ini_config_error_count(ini_config)) {
            INIOUT(printf("Errors detected while parsing: %s\n",
                        ini_config_get_filename(file_ctx)));
            ini_config_get_errors(ini_config, &error_list);
            INIOUT(ini_config_print_errors(stdout, error_list));
            ini_config_free_errors(error_list);
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return error;
        }
    }

    bom = ini_config_get_bom(file_ctx);
    INIOUT(printf("BOM %d\n", bom));

    for (i = 0; i < 4; i++) {

        INIOUT(printf("Processing file %s\n", cmp_files[i]));

        /* Create backup */
        error = ini_config_file_backup(file_ctx,
                                       (builddir == NULL) ? "." : builddir,
                                       baktpl,
                                       &acc,
                                       1000);
        if (error) {
            printf("Failed to create backup file. Error %d.\n", error);
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return error;
        }

        /* Set a new bom */
        error = ini_config_set_bom(file_ctx, bom_ar[i]);
        if (error) {
            printf("Failed to set bom. Error %d.\n", error);
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return error;
        }

        /* Save as a different file */
        INIOUT(printf("Saving as file %s\n", cmp_files[i]));

        snprintf(resname, PATH_MAX, "%s/test_%s",
                 (builddir == NULL) ? "." : builddir,
                 cmp_files[i]);

        error = ini_config_save_as(file_ctx,
                                   resname,
                                   &acc,
                                   ini_config);
        if (error) {
            printf("Failed to save file as %s. Error %d.\n", resname, error);
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return error;
        }

        /* Do comparison of the original file with the created one */
        INIOUT(printf("Comparing file %s\n", cmp_files[i]));

        snprintf(cmpname, PATH_MAX, "%s/ini/ini2.d/%s",
                 (srcdir == NULL) ? "." : srcdir,
                 cmp_files[i]);

        snprintf(command, PATH_MAX * 3, "cmp -l -b %s %s", resname, cmpname);
        error = system(command);
        if ((error) || (WEXITSTATUS(error))) {
            printf("Failed to compare files %d %d.\n",  error,
                   WEXITSTATUS(error));
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return -1;
        }

        /* Change access to the saved file */

        INIOUT(printf("Changing access to file %s\n", cmp_files[i]));

        error = ini_config_change_access(file_ctx,
                                         &new_access);
        if (error) {
            printf("Failed to change access for file %s. Error %d.\n",
                   resname, error);
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return error;
        }

        /* Check that access is as expected */
        INIOUT(printf("Check access to the file %s\n", cmp_files[i]));

        error = ini_config_access_check(file_ctx,
                                        INI_ACCESS_CHECK_MODE,
                                        0,
                                        0,
                                        0660,
                                        0);
        if (error) {
            printf("Failed to check access %s. Error %d.\n", resname, error);
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return error;
        }
    }

    ini_config_file_destroy(file_ctx);
    ini_config_destroy(ini_config);

    INIOUT(printf("<==== END ====>\n"));
    return 0;
}


int main(int argc, char *argv[])
{
    int error = EOK;
    test_fn tests[] = { basic_test,
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
            printf("Failed with error %d!\n", error);
            return error;
        }
    }

    INIOUT(printf("Success!\n"));
    return 0;
}
