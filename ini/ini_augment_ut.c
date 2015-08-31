/*
    INI LIBRARY

    Unit test for the comment object.

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
/* #define TRACE_LEVEL 7 */
#define TRACE_HOME
#include "trace.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "collection_tools.h"
#include "path_utils.h"

int verbose = 0;

#define INIOUT(foo) \
    do { \
        if (verbose) { printf("%30s(%4d): ",__FUNCTION__,__LINE__); foo; } \
    } while(0)

typedef int (*test_fn)(void);

void print_list(struct ref_array *list);
int print_list_to_file(struct ref_array *list,
                       const char *filename,
                       const char *mode);
static int expand_path(const char *path, char **fullname);


/* Construct the full dir path */
static int expand_path(const char *path, char **fullname)
{
    int error = EOK;
    char *dirname = NULL;

    TRACE_FLOW_ENTRY();
    TRACE_INFO_STRING("Input path", path);

    dirname = malloc(PATH_MAX + 1);
    if (!dirname) {
        INIOUT(printf("Failed to allocate memory for file path."));
        return ENOMEM;
    }

    /* Make the path */
    error = make_normalized_absolute_path(dirname,
                                          PATH_MAX,
                                          path);
    if (error) {
        INIOUT(printf("Failed to resolve path %d\n", error));
        free(dirname);
        return error;
    }
    else *fullname = dirname;

    TRACE_INFO_STRING("Output path", *fullname);
    TRACE_FLOW_EXIT();

    return EOK;
}

static int prepare_results(const char *srcdir,
                           const char *srcfile,
                           const char *destfile)
{
    int error = EOK;
    char *exp_src= NULL;
    FILE *fsrc = NULL;
    FILE *fout = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t rd;

    TRACE_FLOW_ENTRY();

    error = expand_path(srcdir, &exp_src);
    if (error) {
        INIOUT(printf("Expand path returned error %d\n", error));
        return error;
    }

    INIOUT(printf("Source file: %s\n", srcfile));
    INIOUT(printf("Output file: %s\n", destfile));

    fsrc = fopen(srcfile, "r");
    if (!fsrc) {
        error = errno;
        free(exp_src);
        INIOUT(printf("Failed to open source file %d\n", error));
        return error;
    }

    fout = fopen(destfile, "w");
    if (!fsrc) {
        error = errno;
        fclose(fsrc);
        free(exp_src);
        INIOUT(printf("Failed to open output file %d\n", error));
        return error;
    }

    INIOUT(printf("Path %s\n", exp_src));

    while ((rd = getline(&line, &len, fsrc)) != -1) {
        if (strchr(line, '%')) fprintf(fout, line, exp_src, "/ini/ini.d");
        else fprintf(fout, "%s", line);
    }

    if (line)
        free(line);

    fclose(fsrc);
    fclose(fout);
    free(exp_src);

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Function to print contents of the list */
void print_list(struct ref_array *list)
{
    uint32_t i = 0;
    char *ret = NULL;
    void *ptr = NULL;

    for (;;) {
        ptr = ref_array_get(list, i, &ret);
        if (ptr) {
            INIOUT(printf("%s\n", ret));
            i++;
        }
        else break;
    }
}

/* Function to print contents of the list */
int print_list_to_file(struct ref_array *list,
                       const char *filename,
                       const char *mode)
{
    uint32_t i = 0;
    char *ret = NULL;
    void *ptr = NULL;
    FILE *file = NULL;

    file = fopen(filename, mode);
    if (file) {
        for (;;) {
            ptr = ref_array_get(list, i, &ret);
            if (ptr) {
                fprintf(file,"%s\n", ret);
                i++;
            }
           else break;
        }
    }
    else {
        printf("Failed to open file for results\n");
        return -1;
    }
    fclose(file);
    return 0;
}


/* Basic test */
static int basic_test(void)
{
    int error = EOK;
    char indir[PATH_MAX];
    char srcname[PATH_MAX];
    char filename[PATH_MAX];
    char resname[PATH_MAX];
    char command[PATH_MAX * 3];
    char *builddir = NULL;
    char *srcdir = NULL;
    struct ini_cfgobj *in_cfg = NULL;
    struct ini_cfgobj *result_cfg = NULL;
    struct ref_array *error_list = NULL;
    struct ref_array *success_list = NULL;
    struct access_check ac = { INI_ACCESS_CHECK_MODE,
                               0,
                               0,
                               0444,
                               0444 };

    /* Match all that do not start with 'r'
    * and end with '.conf' and then match all
    * ending with '.conf' */
    const char *patterns[] = { "#",
                               "^[^r][a-z]*\\.conf$",
                               "^real\\.conf$",
                               NULL };

    /* Match only the config, monitor, domains, services, and provider
    * sections */
    const char *sections[] = { "config",
                               "monitor",
                               "domains",
                               "services",
                               "provider",
                               NULL };


    INIOUT(printf("<==== Start ====>\n"));

    srcdir = getenv("srcdir");

    builddir = getenv("builddir");

    snprintf(indir, PATH_MAX, "%s/ini/ini.d",
                    (srcdir == NULL) ? "." : srcdir);


    /* When run in dev environment there can be some temp files which
     * we need to clean. */
    snprintf(command, PATH_MAX * 3, "rm %s/*~ > /dev/null 2>&1", indir);
    (void)system(command);

    /* Make the file path independent */
    snprintf(srcname, PATH_MAX, "%s/ini/ini.d/merge.validator",
                    (srcdir == NULL) ? "." : srcdir);

    snprintf(filename, PATH_MAX, "%s/merge.validator.in",
                      (builddir == NULL) ? "." : builddir);


    snprintf(resname, PATH_MAX, "%s/merge.validator.out",
                      (builddir == NULL) ? "." : builddir);

    /* Prepare results file so that we can compare results */
    error = prepare_results(srcdir, srcname, filename);
    if (error) {
        INIOUT(printf("Failed to results file. Error %d.\n", error));
        return error;
    }

    /* Create config collection */
    error = ini_config_create(&in_cfg);
    if (error) {
        INIOUT(printf("Failed to create collection. Error %d.\n", error));
        return error;
    }

    error = ini_config_augment(in_cfg,
                               indir,
                               patterns,
                               sections,
                               &ac,
                               INI_STOP_ON_NONE,
                               INI_MV1S_DETECT|INI_MV2S_DETECT|INI_MS_DETECT,
                               INI_PARSE_NOSPACE|INI_PARSE_NOTAB,
                               INI_MV2S_DETECT|INI_MS_DETECT,
                               &result_cfg,
                               &error_list,
                               &success_list);
    if (error) {
        INIOUT(printf("Augmentation failed with error %d!\n", error));
    }

    print_list(error_list);
    print_list(success_list);

    if (!result_cfg) {
        error = -1;
        printf("Configuration is empty.\n");
    }
    else INIOUT(col_debug_collection(result_cfg->cfg, COL_TRAVERSE_DEFAULT));

    /* Print results to file */
    if ((print_list_to_file(error_list, resname, "w")) ||
        (print_list_to_file(success_list, resname, "a"))) {
        printf("Failed to save results in %s.\n",  resname);
        ref_array_destroy(error_list);
        ref_array_destroy(success_list);
        ini_config_destroy(in_cfg);
        ini_config_destroy(result_cfg);
        return -1;
    }

    /* NOTE: The order of the scanning of the files in the ini.d directory
     * is not predicatble so before comparing the results we have to sort
     * them since otherwise the projected output and real output might
     * not match.
     */

    snprintf(command, PATH_MAX * 3, "sort %s > %s2", filename, filename);
    error = system(command);
    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run first sort command %d %d.\n",  error,
               WEXITSTATUS(error));
        ref_array_destroy(error_list);
        ref_array_destroy(success_list);
        ini_config_destroy(in_cfg);
        ini_config_destroy(result_cfg);
        return -1;
    }

    snprintf(command, PATH_MAX * 3, "sort %s > %s2", resname, resname);
    error = system(command);
    error = system(command);
    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run second sort command %d %d.\n",  error,
               WEXITSTATUS(error));
        ref_array_destroy(error_list);
        ref_array_destroy(success_list);
        ini_config_destroy(in_cfg);
        ini_config_destroy(result_cfg);
        return -1;
    }


    snprintf(command, PATH_MAX * 3, "diff -q %s2 %s2", filename, resname);
    error = system(command);
    INIOUT(printf("Comparison of %s %s returned: %d\n",
                  filename, resname, error));

    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run diff command %d %d.\n",  error,
               WEXITSTATUS(error));
        ref_array_destroy(error_list);
        ref_array_destroy(success_list);
        ini_config_destroy(in_cfg);
        ini_config_destroy(result_cfg);
        return -1;
    }

    /* Cleanup */
    ref_array_destroy(error_list);
    ref_array_destroy(success_list);
    ini_config_destroy(in_cfg);
    ini_config_destroy(result_cfg);

    INIOUT(printf("<==== End ====>\n"));

    return error;
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
