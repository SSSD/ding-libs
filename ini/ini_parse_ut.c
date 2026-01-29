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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "ini_defines.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "ini_valueobj.h"
#include "simplebuffer.h"
#include "path_utils.h"
#define TRACE_HOME
#include "trace.h"
#include "collection_tools.h"

int verbose = 0;
char *confdir = NULL;

#define NUM_TESTS 14
#define VAL_SIZE 100

#define INIOUT(foo) \
    do { \
        if (verbose) foo; \
    } while(0)

typedef int (*test_fn)(void);

static int test_one_file(const char *in_filename,
                         const char *out_filename,
                         int edge,
                         int in_mem)
{
    int error = EOK;
    struct ini_cfgfile *file_ctx = NULL;
    FILE *ff = NULL;
    struct ini_cfgobj *ini_config = NULL;
    struct ini_cfgobj *ini_copy = NULL;
    char **error_list = NULL;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;
    uint32_t stream_len = 0;
    void *stream_data = NULL;
    struct stat file_stats;
    int stat_ret = 0;
    FILE *file = NULL;

    INIOUT(printf("<==== Testing file %s ====>\n", in_filename));

    /* Create config collection */
    error = ini_config_create(&ini_config);
    if (error) {
        printf("Failed to create collection. Error %d.\n", error);
        return error;
    }

    if (in_mem) {
        /* Get file stats */
        errno = 0;
        stat_ret = stat(in_filename, &file_stats);
        if (stat_ret == -1) {
            error = errno;
            printf("Failed to get file stats. Error %d.\n", error);
            ini_config_destroy(ini_config);
            return error;
        }
        /* Allocate memory to store file */
        errno = 0;
        stream_len = file_stats.st_size;
        stream_data = malloc(file_stats.st_size + 1);
        if (!stream_data) {
            error = errno;
            printf("Failed to allocate memory for stream. Error %d.\n", error);
            ini_config_destroy(ini_config);
            return error;
        }
        *((unsigned char *)(stream_data) + stream_len) = '\0';
        /* Open file */
        errno = 0;
        file = fopen(in_filename, "r");
        if (!stream_data) {
            error = errno;
            printf("Failed to open file to prepare stream. Error %d.\n", error);
            free(stream_data);
            ini_config_destroy(ini_config);
            return error;
        }
        /* Read file into memory */
        errno = 0;
        fread(stream_data, stream_len, 1, file);
        error = ferror(file);
        if (error) {
            printf("Failed to read stream data. Error %d.\n", error);
            free(stream_data);
            fclose(file);
            ini_config_destroy(ini_config);
            return error;
        }
        fclose(file);

        INIOUT(printf("Data len %u\n", stream_len));
        INIOUT(printf("Data:\n%s\n", (char *)stream_data));

        error = ini_config_file_from_mem(stream_data,
                                         stream_len,
                                         &file_ctx);
        if (error) {
            printf("Failed to open from memory. Error %d.\n", error);
            free(stream_data);
            ini_config_destroy(ini_config);
            return error;
        }
    }
    else {
        error = ini_config_file_open(in_filename,
                                     0, /* TBD */
                                     &file_ctx);
        if (error) {
            printf("Failed to open file %s for reading. Error %d.\n",
                   in_filename, error);
            ini_config_destroy(ini_config);
            return error;
        }
    }

    error = ini_config_parse(file_ctx,
                             INI_STOP_ON_NONE,
                             0, /* TBD */
                             0,
                             ini_config);
    if (error) {
        INIOUT(printf("Failed to parse configuration. Error %d.\n", error));

        if (ini_config_error_count(ini_config)) {
            if (in_mem) {
                INIOUT(printf("Errors detected while parsing"
                              " configuration stored in memory: %s\n",
                              in_filename));
            }
            else {
                INIOUT(printf("Errors detected while parsing: %s\n",
                        ini_config_get_filename(file_ctx)));
            }
            ini_config_get_errors(ini_config, &error_list);
            INIOUT(ini_config_print_errors(stdout, error_list));
            ini_config_free_errors(error_list);
        }
        /* If we are testing memory mapped, return error */
        if (in_mem) {
            free(stream_data);
            ini_config_file_destroy(file_ctx);
            ini_config_destroy(ini_config);
            return error;
        }
    }

    if (in_mem) free(stream_data);
    ini_config_file_destroy(file_ctx);

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

    error = ini_config_set_wrap(ini_config, edge);
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
        printf("Failed to open file [%s] for writing. Error %d.\n",
               out_filename, error);
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
static int read_save_test(void)
{
    int error = EOK;
    int i = 0;
    int edge = 5;
    char infile[PATH_MAX];
    char outfile[PATH_MAX];
    char *srcdir = NULL;
    const char *files[] = { "real",
                            "mysssd",
                            "ipa",
                            "test",
                            "smerge",
                            "real8",
                            "real16be",
                            "real16le",
                            "real32be",
                            "real32le",
                            "symbols",
                            NULL };

    INIOUT(printf("<==== Read save test ====>\n"));

    srcdir = getenv("srcdir");

    while(files[i]) {
        for ( edge = 10; edge < 100; edge +=19) {
            snprintf(infile, PATH_MAX, "%s/ini/ini.d/%s.conf",
                    (srcdir == NULL) ? "." : srcdir, files[i]);
            snprintf(outfile, PATH_MAX, "./%s_%d.conf.out", files[i], edge);
            error = test_one_file(infile, outfile, edge, 0);
            INIOUT(printf("Test for file: %s returned %d\n", files[i], error));
            if (error) return error;
        }
        i++;
    }

    INIOUT(printf("<==== Read save test end ====>\n"));

    return EOK;
}


/* Run tests for multiple files */
static int read_mem_test(void)
{
    int error = EOK;
    int i = 0;
    int edge = 5;
    char infile[PATH_MAX];
    char outfile[PATH_MAX];
    char *srcdir = NULL;
    const char *files[] = { "real",
                            "mysssd",
                            "ipa",
                            "test",
                            "smerge",
                            "real8",
                            "real16be",
                            "real16le",
                            "real32be",
                            "real32le",
                            "symbols",
                            "new_line",
                            NULL };

    INIOUT(printf("<==== Read mem test ====>\n"));

    srcdir = getenv("srcdir");

    while(files[i]) {
        for ( edge = 15; edge < 100; edge +=25) {
            snprintf(infile, PATH_MAX, "%s/ini/ini.d/%s.conf",
                    (srcdir == NULL) ? "." : srcdir, files[i]);
            snprintf(outfile, PATH_MAX, "./%s_%d.conf.mem.out", files[i], edge);
            error = test_one_file(infile, outfile, edge, 1);
            INIOUT(printf("Test for file: %s returned %d\n", files[i], error));
            if ((error) && (strncmp(files[i], "test", 4) != 0)) return error;
        }
        i++;
    }

    INIOUT(printf("<==== Read mem test end ====>\n"));

    return EOK;
}


/* Run tests for multiple files */
static int read_again_test(void)
{
    int error = EOK;
    int i = 0;
    int edge = 5;
    char infile[PATH_MAX];
    char outfile[PATH_MAX];
    char command[PATH_MAX * 3];
    const char *files[] = { "real",
                            "mysssd",
                            "ipa",
                            "test",
                            "smerge",
                            "real8",
                            "real16be",
                            "real16le",
                            "real32be",
                            "real32le",
                            "symbols",
                            NULL };

    INIOUT(printf("<==== Read again test ====>\n"));

    while(files[i]) {
        for ( edge = 10; edge < 100; edge +=19) {

            snprintf(infile, PATH_MAX, "./%s_%d.conf.out", files[i], edge);
            snprintf(outfile, PATH_MAX, "./%s_%d.conf.2.out", files[i], edge);

            error = test_one_file(infile, outfile, edge, 0);
            INIOUT(printf("Test for file: %s returned %d\n", files[i], error));
            if (error) break;
            snprintf(command, PATH_MAX * 3, "diff -q %s %s", infile, outfile);
            error = system(command);
            INIOUT(printf("Comparison of %s %s returned: %d\n",
                          infile, outfile, error));
            if ((error) || (WEXITSTATUS(error))) {
                printf("Failed to run copy command %d %d.\n",  error,
                       WEXITSTATUS(error));
                error = -1;
                break;
            }
        }

        i++;
    }

    INIOUT(printf("<==== Read again test end ====>\n"));

    return error;
}

static int create_expect(const char *checkname)
{
    FILE *ff;
    int error = EOK;

    errno = 0;
    ff = fopen(checkname, "w");
    if(!ff) {
        error = errno;
        printf("Failed to open file %s for writing. Error %d.\n",
               checkname, error);
        return error;
    }

    /* Ovewrite */
    fprintf(ff,"#Hoho section\n");
    fprintf(ff,"[hoho]\n");
    fprintf(ff,"#Hoho value\n");
    fprintf(ff,"val = hoho\n");
    fprintf(ff,"#End of hoho\n");
    fprintf(ff,"#Start of section\n");
    fprintf(ff,"[foo]\n");
    fprintf(ff,"#Second value\n");
    fprintf(ff,"bar = second value\n");
    fprintf(ff,"#End of section\n");
    /* Preserve */
    fprintf(ff,"#Hoho section\n");
    fprintf(ff,"[hoho]\n");
    fprintf(ff,"#Hoho value\n");
    fprintf(ff,"val = hoho\n");
    fprintf(ff,"#End of hoho\n");
    fprintf(ff,"#Start of section\n");
    fprintf(ff,"[foo]\n");
    fprintf(ff,"#First value\n");
    fprintf(ff,"bar = first value\n");
    fprintf(ff,"#End of section\n");
    /* Allow */
    fprintf(ff,"#Hoho section\n");
    fprintf(ff,"[hoho]\n");
    fprintf(ff,"#Hoho value\n");
    fprintf(ff,"val = hoho\n");
    fprintf(ff,"#End of hoho\n");
    fprintf(ff,"#Start of section\n");
    fprintf(ff,"[foo]\n");
    fprintf(ff,"#First value\n");
    fprintf(ff,"bar = first value\n");
    fprintf(ff,"#Second value\n");
    fprintf(ff,"bar = second value\n");
    fprintf(ff,"#End of section\n");
    /* Detect */
    fprintf(ff,"#Hoho section\n");
    fprintf(ff,"[hoho]\n");
    fprintf(ff,"#Hoho value\n");
    fprintf(ff,"val = hoho\n");
    fprintf(ff,"#End of hoho\n");
    fprintf(ff,"#Start of section\n");
    fprintf(ff,"[foo]\n");
    fprintf(ff,"#First value\n");
    fprintf(ff,"bar = first value\n");
    fprintf(ff,"#Second value\n");
    fprintf(ff,"bar = second value\n");
    fprintf(ff,"#End of section\n");

    fclose(ff);

    return EOK;
}

/* Check merge modes */
static int merge_values_test(void)
{
    int error = EOK;
    int i;
    struct ini_cfgfile *file_ctx = NULL;
    FILE *ff = NULL;
    struct ini_cfgobj *ini_config = NULL;
    char **error_list = NULL;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;
    uint32_t mflags[] = { INI_MV1S_OVERWRITE,
                          INI_MV1S_ERROR,
                          INI_MV1S_PRESERVE,
                          INI_MV1S_ALLOW,
                          INI_MV1S_DETECT };

    const char *mstr[] = { "OVERWRITE",
                           "ERROR",
                           "PRESERVE",
                           "ALLOW",
                           "DETECT" };

    char filename[PATH_MAX];
    const char *resname = "./merge.conf.out";
    const char *checkname = "./expect.conf.out";
    char command[PATH_MAX * 3];
    char *srcdir = NULL;

    INIOUT(printf("<==== Merge values test ====>\n"));

    srcdir = getenv("srcdir");

    snprintf(filename, PATH_MAX, "%s/ini/ini.d/foo.conf.in",
            (srcdir == NULL) ? "." : srcdir);

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    for (i = 0; i < 5; i++) {

        INIOUT(printf("<==== Testing mode %s  ====>\n", mstr[i]));

        /* Create config collection */
        ini_config = NULL;
        error = ini_config_create(&ini_config);
        if (error) {
            printf("Failed to create collection. Error %d.\n", error);
            simplebuffer_free(sbobj);
            return error;
        }

        file_ctx = NULL;
        error = ini_config_file_open(filename,
                                     0, /* TBD */
                                     &file_ctx);
        if (error) {
            printf("Failed to open file %s for reading. Error %d.\n",
                   filename, error);
            printf("Src dir is [%s].\n", (srcdir == NULL) ?
                   "NOT DEFINED" : srcdir);
            ini_config_destroy(ini_config);
            simplebuffer_free(sbobj);
            return error;
        }

        error = ini_config_parse(file_ctx,
                                 INI_STOP_ON_ANY,
                                 mflags[i],
                                 0,
                                 ini_config);
        if (error) {
            INIOUT(printf("Failed to parse configuration. Error %d.\n",
                          error));

            if (ini_config_error_count(ini_config)) {
                INIOUT(printf("Errors detected while parsing: %s\n",
                       ini_config_get_filename(file_ctx)));
                ini_config_get_errors(ini_config, &error_list);
                INIOUT(ini_config_print_errors(stdout, error_list));
                ini_config_free_errors(error_list);
            }

            if (((mflags[i] != INI_MV1S_ERROR) &&
                 (mflags[i]!= INI_MV1S_DETECT)) ||
                ((mflags[i] = INI_MV1S_ERROR) && (error != EEXIST)) ||
                ((mflags[i] = INI_MV1S_DETECT) && (error != EEXIST))) {
                printf("This is unexpected error %d in mode %d\n",
                       error, mflags[i]);
                ini_config_destroy(ini_config);
                simplebuffer_free(sbobj);
                ini_config_file_destroy(file_ctx);
                return error;
            }
            /* We do not return here intentionally */
        }

        ini_config_file_destroy(file_ctx);

        INIOUT(col_debug_collection(ini_config->cfg, COL_TRAVERSE_DEFAULT));

        error = ini_config_serialize(ini_config, sbobj);
        if (error != EOK) {
            printf("Failed to serialize configuration. Error %d.\n", error);
            ini_config_destroy(ini_config);
            simplebuffer_free(sbobj);
            return error;
        }

        ini_config_destroy(ini_config);
    }

    errno = 0;
    ff = fopen(resname, "w");
    if(!ff) {
        error = errno;
        printf("Failed to open file for writing. Error %d.\n", error);
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
            fclose(ff);
            return error;
        }
    }

    simplebuffer_free(sbobj);
    fclose(ff);

    error = create_expect(checkname);
    if (error) {
        printf("Failed to create file with expected contents %d.\n",  error);
        return error;
    }

    snprintf(command, PATH_MAX * 3, "diff -q %s %s", resname, checkname);
    error = system(command);
    INIOUT(printf("Comparison of %s %s returned: %d\n",
                  resname, checkname, error));
    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run copy command %d %d.\n",  error,
               WEXITSTATUS(error));
        return -1;
    }

    INIOUT(printf("<==== Merge values test end ====>\n"));

    return error;
}

/* Check merge modes */
static int merge_section_test(void)
{
    int error = EOK;
    int i, j;
    struct ini_cfgfile *file_ctx = NULL;
    FILE *ff = NULL;
    struct ini_cfgobj *ini_config = NULL;
    char **error_list = NULL;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;
    uint32_t msecflags[] = { INI_MS_MERGE,
                             INI_MS_ERROR,
                             INI_MS_OVERWRITE,
                             INI_MS_PRESERVE,
                             INI_MS_DETECT };

    uint32_t mflags[] = { INI_MV2S_OVERWRITE,
                          INI_MV2S_ERROR,
                          INI_MV2S_PRESERVE,
                          INI_MV2S_ALLOW,
                          INI_MV2S_DETECT };

    const char *secmstr[] = { "MERGE",
                              "ERROR",
                              "OVERWRITE",
                              "PRESERVE",
                              "DETECT" };

    const char *mstr[] = { "OVERWRITE",
                           "ERROR",
                           "PRESERVE",
                           "ALLOW",
                           "DETECT" };

    char filename[PATH_MAX];
    char checkname[PATH_MAX];
    char resname[PATH_MAX];
    char command[PATH_MAX * 3];
    char mode[VAL_SIZE];
    char *srcdir = NULL;
    char *builddir = NULL;


    INIOUT(printf("<==== Merge section test ====>\n"));

    srcdir = getenv("srcdir");
    builddir = getenv("builddir");
    snprintf(filename, PATH_MAX, "%s/ini/ini.d/smerge.conf",
                      (srcdir == NULL) ? "." : srcdir);
    snprintf(checkname, PATH_MAX, "%s/ini/ini.d/sexpect.conf",
                      (srcdir == NULL) ? "." : srcdir);
    snprintf(resname, PATH_MAX, "%s/smerge.conf.out",
                      (builddir == NULL) ? "." : builddir);

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {

            INIOUT(printf("<==== Testing mode %s + %s ====>\n",
                          secmstr[i], mstr[j]));

            snprintf(mode, VAL_SIZE, "# Section mode: %s, value mode: %s\n",
                          secmstr[i], mstr[j]);

            error = simplebuffer_add_str(sbobj,
                                         mode,
                                         strlen(mode),
                                         VAL_SIZE);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to add string.",
                                   error);
                simplebuffer_free(sbobj);
                return error;
            }

            /* Create config collection */
            ini_config = NULL;
            error = ini_config_create(&ini_config);
            if (error) {
                printf("Failed to create collection. "
                       "Error %d.\n", error);
                simplebuffer_free(sbobj);
                return error;
            }

            file_ctx = NULL;
            error = ini_config_file_open(filename,
                                         0, /* TBD */
                                         &file_ctx);
            if (error) {
                printf("Failed to open file %s for reading. "
                       "Error %d.\n", filename, error);
                printf("Source is %s.\n", (srcdir == NULL) ?
                       "NOT Defined" : srcdir);
                ini_config_destroy(ini_config);
                simplebuffer_free(sbobj);
                return error;
            }

            error = ini_config_parse(file_ctx,
                                     INI_STOP_ON_ANY,
                                     msecflags[i] | mflags[j],
                                     0,
                                     ini_config);
            if (error) {
                INIOUT(printf("Failed to parse configuration. "
                              "Error %d.\n", error));

                if (ini_config_error_count(ini_config)) {
                    INIOUT(printf("Errors detected while parsing: %s\n",
                           ini_config_get_filename(file_ctx)));
                    ini_config_get_errors(ini_config, &error_list);
                    INIOUT(ini_config_print_errors(stdout, error_list));
                    ini_config_free_errors(error_list);
                }

                if (((msecflags[i] == INI_MS_ERROR) &&
                     (error == EEXIST)) ||
                    ((msecflags[i] == INI_MS_DETECT) &&
                     (error == EEXIST)) ||
                    ((msecflags[i] == INI_MS_MERGE) &&
                     ((mflags[j] == INI_MV2S_ERROR) ||
                      (mflags[j] == INI_MV2S_DETECT)) &&
                      (error == EEXIST))) {
                    INIOUT(printf("This is an expected error "
                                  "%d in mode %d + %d\n",
                                  error,
                                  msecflags[i],
                                  mflags[j]));
                    /* We do not return here intentionally */
                }
                else {
                    printf("This is unexpected error %d in mode %d + %d\n",
                            error, msecflags[i], mflags[j]);
                    ini_config_destroy(ini_config);
                    simplebuffer_free(sbobj);
                    ini_config_file_destroy(file_ctx);
                    return error;
                }
            }

            ini_config_file_destroy(file_ctx);

            INIOUT(col_debug_collection(ini_config->cfg,
                   COL_TRAVERSE_DEFAULT));

            error = ini_config_serialize(ini_config, sbobj);
            if (error != EOK) {
                printf("Failed to serialize configuration. "
                       "Error %d.\n", error);
                ini_config_destroy(ini_config);
                simplebuffer_free(sbobj);
                return error;
            }

            ini_config_destroy(ini_config);
        }
    }

    errno = 0;
    ff = fopen(resname, "w");
    if(!ff) {
        error = errno;
        printf("Failed to open file for writing. Error %d.\n", error);
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
            fclose(ff);
            return error;
        }
    }

    simplebuffer_free(sbobj);
    fclose(ff);

    snprintf(command, PATH_MAX * 3, "diff -q %s %s", resname, checkname);
    error = system(command);
    INIOUT(printf("Comparison of %s %s returned: %d\n",
                  resname, checkname, error));

    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run diff command %d %d.\n",  error,
               WEXITSTATUS(error));
        return -1;
    }

    INIOUT(printf("<==== Merge section test end ====>\n"));

    return error;
}

static int read_one_file(char *name,
                         struct ini_cfgobj *ini_config,
                         uint32_t collision_flags)
{
    int error = EOK;
    struct ini_cfgfile *file_ctx = NULL;
    char **error_list = NULL;

    INIOUT(printf("Reading file %s\n", name));

    file_ctx = NULL;
    error = ini_config_file_open(name,
                                 0,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file %s for reading. "
                "Error %d.\n", name, error);
        return error;
    }

    INIOUT(printf("Parsing file %s\n", name));

    error = ini_config_parse(file_ctx,
                             INI_STOP_ON_ANY,
                             collision_flags,
                             0,
                             ini_config);
    if (error) {
        INIOUT(printf("Failed to parse configuration. "
                      "Error %d.\n", error));

        if (ini_config_error_count(ini_config)) {
            INIOUT(printf("Errors detected while parsing: %s\n",
                   ini_config_get_filename(file_ctx)));
            ini_config_get_errors(ini_config, &error_list);
            INIOUT(ini_config_print_errors(stdout, error_list));
            ini_config_free_errors(error_list);
        }
        ini_config_file_destroy(file_ctx);
        return error;
    }

    ini_config_file_destroy(file_ctx);

    INIOUT(printf("Successfully parsed file %s\n", name));
    return EOK;
}

/* Check merge modes */
static int merge_file_test(void)
{
    int error = EOK;
    int i, j;
    struct ini_cfgobj *ini_config_first = NULL;
    struct ini_cfgobj *ini_config_second = NULL;
    struct ini_cfgobj *ini_config_result = NULL;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;
    FILE *ff = NULL;
    uint32_t msecflags[] = { INI_MS_MERGE,
                             INI_MS_ERROR,
                             INI_MS_OVERWRITE,
                             INI_MS_PRESERVE,
                             INI_MS_DETECT };

    uint32_t m2flags[] = { INI_MV2S_OVERWRITE,
                           INI_MV2S_ERROR,
                           INI_MV2S_PRESERVE,
                           INI_MV2S_ALLOW,
                           INI_MV2S_DETECT };

    uint32_t m1flags[] = { INI_MV1S_OVERWRITE,
                           INI_MV1S_ERROR,
                           INI_MV1S_PRESERVE,
                           INI_MV1S_ALLOW,
                           INI_MV1S_DETECT };

    const char *secmstr[] = { "MERGE",
                              "ERROR",
                              "OVERWRITE",
                              "PRESERVE",
                              "DETECT" };

    const char *mstr[] = { "OVERWRITE",
                           "ERROR",
                           "PRESERVE",
                           "ALLOW",
                           "DETECT" };

    char firstname[PATH_MAX];
    char secondname[PATH_MAX];
    char resname[PATH_MAX];
    char checkname[PATH_MAX];
    char command[PATH_MAX * 3];
    char msg[VAL_SIZE];
    char mode[VAL_SIZE];
    char *srcdir = NULL;
    char *builddir = NULL;
    uint32_t collision_flags;
    uint32_t ms_subst;
    uint32_t mv1s_subst;
    uint32_t mv2s_subst;

    INIOUT(printf("<==== Merge file test ====>\n"));

    srcdir = getenv("srcdir");
    builddir = getenv("builddir");
    snprintf(firstname, PATH_MAX, "%s/ini/ini.d/first.conf",
                      (srcdir == NULL) ? "." : srcdir);

    snprintf(secondname, PATH_MAX, "%s/ini/ini.d/second.conf",
                      (srcdir == NULL) ? "." : srcdir);

    snprintf(checkname, PATH_MAX, "%s/ini/ini.d/mergecheck.conf",
                      (srcdir == NULL) ? "." : srcdir);

    snprintf(resname, PATH_MAX, "%s/mergecheck.conf.out",
                      (builddir == NULL) ? "." : builddir);

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate dynamic string.", error);
        return error;
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {

            INIOUT(printf("<==== Testing mode %s + %s ====>\n",
                          secmstr[i], mstr[j]));

            snprintf(mode, VAL_SIZE, "# Section mode: %s, value mode: %s\n",
                          secmstr[i], mstr[j]);

            error = simplebuffer_add_str(sbobj,
                                         mode,
                                         strlen(mode),
                                         VAL_SIZE);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to add string.",
                                   error);
                simplebuffer_free(sbobj);
                return error;
            }

            /* Create first config collection */
            ini_config_first = NULL;
            error = ini_config_create(&ini_config_first);
            if (error) {
                printf("Failed to create collection. "
                       "Error %d.\n", error);
                simplebuffer_free(sbobj);
                return error;
            }

            /* Create second config collection */
            ini_config_second = NULL;
            error = ini_config_create(&ini_config_second);
            if (error) {
                printf("Failed to create collection. "
                       "Error %d.\n", error);
                ini_config_destroy(ini_config_first);
                simplebuffer_free(sbobj);
                return error;
            }

            /* IMPORTANT: Use same collision flags for reading
             * of the files and then merging.
             * Mixing the flags would lead to strange results
             * that would be hard to debug.
             */
            /* However here for purely testing purposes
             * we will not use error modes in parsing
             * otherwise we will not be able to try to merge.
             * Instead we replace the error and detect modes
             * with allow or merge mode.
             */
            /* The test actually does not fail in the case of
             * PRESERVE + ERROR becuase it should fail at
             * the stage of reading file but we suppress
             * it so we can try the merge.
             * As a result the mode PRESERVE + ERROR
             * acts as PRESERVE + ALLOW and does not return an error.
             * The same thing happens with PRESERVE + DETECT mode.
             * It might be confusing if someone tries to decipher
             * the tests, so this comment should help.
             */
            if ((msecflags[i] == INI_MS_ERROR) ||
                (msecflags[i] == INI_MS_DETECT)) {
                ms_subst = msecflags[i];
            }
            else {
                ms_subst = INI_MS_MERGE;
            }

            if ((m2flags[j] == INI_MV2S_ERROR) ||
                (m2flags[j] == INI_MV2S_DETECT)) {
                mv1s_subst = INI_MV1S_ALLOW;
                mv2s_subst = INI_MV2S_ALLOW;
            }
            else {
                mv1s_subst = m1flags[j];
                mv2s_subst = m2flags[j];
            }

            collision_flags = mv1s_subst | mv2s_subst | ms_subst;

            error = read_one_file(firstname,
                                  ini_config_first,
                                  collision_flags);
            if (error) {
                printf("Failed to read %s. "
                       "Error %d.\n", firstname, error);
                printf("Source is %s.\n", (srcdir == NULL) ?
                        "NOT Defined" : srcdir);
                ini_config_destroy(ini_config_first);
                ini_config_destroy(ini_config_second);
                simplebuffer_free(sbobj);
                return error;
            }

            error = read_one_file(secondname,
                                  ini_config_second,
                                  collision_flags);
            if (error) {
                printf("Failed to read %s. "
                       "Error %d.\n", secondname, error);
                printf("Source is %s.\n", (srcdir == NULL) ?
                        "NOT Defined" : srcdir);
                ini_config_destroy(ini_config_first);
                ini_config_destroy(ini_config_second);
                simplebuffer_free(sbobj);
                return error;
            }

            INIOUT(col_debug_collection(ini_config_first->cfg,
                   COL_TRAVERSE_ONELEVEL));
            INIOUT(col_debug_collection(ini_config_second->cfg,
                   COL_TRAVERSE_ONELEVEL));

            ini_config_result = NULL;
            error = ini_config_merge(ini_config_first,
                                     ini_config_second,
                                     msecflags[i] | m2flags[j] | m1flags[j],
                                     &ini_config_result);
            if (error) {
                if ((error == EEXIST) &&
                    ((msecflags[i] == INI_MS_ERROR) ||
                     (m2flags[j] == INI_MV2S_ERROR))) {
                    snprintf(msg, sizeof(msg) -1,
                                  "# This is an expected error "
                                  "%d in mode %d + %d + %d\n\n",
                                  error,
                                  msecflags[i],
                                  m2flags[j],
                                  m1flags[j]);
                    INIOUT(printf("%s", msg));
                    error = simplebuffer_add_str(sbobj,
                                                 msg,
                                                 strlen(msg),
                                                 VAL_SIZE);
                    if (error) {
                        TRACE_ERROR_NUMBER("Failed to add string.",
                                           error);
                        ini_config_destroy(ini_config_first);
                        ini_config_destroy(ini_config_second);
                        simplebuffer_free(sbobj);
                        return error;
                    }

                    ini_config_destroy(ini_config_first);
                    ini_config_destroy(ini_config_second);
                    continue;
                }
                else if ((error == EEXIST) &&
                         ((msecflags[i] == INI_MS_DETECT) ||
                          ((msecflags[i] != INI_MS_ERROR) &&
                           (m2flags[j] == INI_MV2S_DETECT)))) {
                    snprintf(msg, sizeof(msg) -1,
                                  "# This is an expected error "
                                  "%d in mode %d + %d + %d\n\n",
                                  error,
                                  msecflags[i],
                                  m2flags[j],
                                  m1flags[j]);
                    INIOUT(printf("%s", msg));
                    error = simplebuffer_add_str(sbobj,
                                                 msg,
                                                 strlen(msg),
                                                 VAL_SIZE);
                    if (error) {
                        TRACE_ERROR_NUMBER("Failed to add string.",
                                           error);
                        ini_config_destroy(ini_config_first);
                        ini_config_destroy(ini_config_second);
                        simplebuffer_free(sbobj);
                        return error;
                    }
                    /* Falling throught here */
                }
                else {
                    TRACE_ERROR_NUMBER("Failed to merge.",
                                       error);
                    ini_config_destroy(ini_config_first);
                    ini_config_destroy(ini_config_second);
                    simplebuffer_free(sbobj);
                    return error;
                }
            }

            INIOUT(col_debug_collection(ini_config_result->cfg,
                   COL_TRAVERSE_DEFAULT));

            error = ini_config_serialize(ini_config_result, sbobj);
            if (error) {
                printf("Failed to serialize configuration. "
                       "Error %d.\n", error);
                ini_config_destroy(ini_config_first);
                ini_config_destroy(ini_config_second);
                ini_config_destroy(ini_config_result);
                simplebuffer_free(sbobj);
                return error;
            }

            ini_config_destroy(ini_config_first);
            ini_config_destroy(ini_config_second);
            ini_config_destroy(ini_config_result);
        }
    }

    errno = 0;
    ff = fopen(resname, "w");
    if(!ff) {
        error = errno;
        printf("Failed to open file for writing. Error %d.\n", error);
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
            fclose(ff);
            return error;
        }
    }

    simplebuffer_free(sbobj);
    fclose(ff);

    snprintf(command,PATH_MAX * 3, "diff -q %s %s", resname, checkname);
    error = system(command);
    INIOUT(printf("Comparison of %s %s returned: %d\n",
                  resname, checkname, error));

    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run diff command %d %d.\n",  error,
               WEXITSTATUS(error));
        return -1;
    }

    INIOUT(printf("<==== Merge section file end ====>\n"));

    return EOK;
}

static int startup_test(void)
{
    int error = EOK;
    struct ini_cfgfile *file_ctx = NULL;
    struct ini_cfgobj *ini_config = NULL;
    char **error_list = NULL;
    char infile[PATH_MAX];
    char outfile[PATH_MAX];
    char command[PATH_MAX * 3];
    char *srcdir = NULL;
    char *builddir;
    const struct stat *file_stat;

    INIOUT(printf("<==== Startup test ====>\n"));

    srcdir = getenv("srcdir");
    snprintf(infile, PATH_MAX, "%s/ini/ini.d/foo.conf.in",
                     (srcdir == NULL) ? "." : srcdir);
    builddir = getenv("builddir");
    snprintf(outfile, PATH_MAX, "%s/foo.conf",
                      (builddir == NULL) ? "." : builddir);

    snprintf(command, PATH_MAX * 3, "cp %s %s", infile, outfile);
    INIOUT(printf("Running command '%s'\n", command));

    error = system(command);
    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run copy command %d %d.\n",  error,
               WEXITSTATUS(error));
        return -1;
    }

    INIOUT(printf("Running chmod 660 on file '%s'\n", outfile));
    error = chmod(outfile, S_IRUSR | S_IWUSR);
    if(error) {
        error = errno;
        printf("Failed to run chmod command %d.\n",  error);
        return error;
    }

    /* Open config file not collecting stats */
    error = ini_config_file_open(outfile,
                                 0,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file %s for reading. Error %d.\n",
               outfile, error);
        return error;
    }

    file_stat = ini_config_get_stat(file_ctx);
    if (file_stat) {
        printf("Expected NULL got not NULL!\n");
        return EINVAL;
    }

    /* We will check just permissions here. */
    error = ini_config_access_check(file_ctx,
                                INI_ACCESS_CHECK_MODE, /* add uid & gui flags
                                                        * in real case
                                                        */
                                0, /* <- will be real uid in real case */
                                0, /* <- will be real gid in real case */
                                0440, /* Checking for r--r----- */
                                0);
    /* This check is expected to fail since
     * we did not collect stats
     */
    ini_config_file_destroy(file_ctx);

    if (!error) {
        printf("Expected error got success!\n");
        return EACCES;
    }


    /* Open config file */
    error = ini_config_file_open(outfile,
                                 INI_META_STATS,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file %s for reading. Error %d.\n",
               outfile, error);
        return error;
    }

    /* Get stats */
    file_stat = ini_config_get_stat(file_ctx);
    if (!file_stat) {
        printf("Expected not NULL got NULL!\n");
        return EINVAL;
    }

    INIOUT(printf("File was modified at: %d seconds since Jan 1 1970.\n",
                  (int)(file_stat->st_mtime)));

    /* We will check just permissions here. */
    error = ini_config_access_check(file_ctx,
                                INI_ACCESS_CHECK_MODE, /* add uid & gui flags
                                                        * in real case
                                                        */
                                0, /* <- will be real uid in real case */
                                0, /* <- will be real gid in real case */
                                0440, /* Checking for r--r----- */
                                0);
    /* This check is expected to fail since
     * the actual permissions on the test file are: rw-------
     */

    if (!error) {
        printf("Expected error got success!\n");
        ini_config_file_destroy(file_ctx);
        return EACCES;
    }

    error = ini_config_access_check(
                        file_ctx,
                        INI_ACCESS_CHECK_MODE, /* add uid & gui flags
                                                * in real case
                                                */
                        0, /* <- will be real uid in real case */
                        0, /* <- will be real gid in real case */
                        0600, /* Checkling for rw------- */
                        0);

    if (error) {
        printf("Access check failed %d!\n", error);
        ini_config_file_destroy(file_ctx);
        return EACCES;
    }

    /* Create config object */
    error = ini_config_create(&ini_config);
    if (error) {
        printf("Failed to create collection. Error %d.\n", error);
        ini_config_file_destroy(file_ctx);
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
        }
        /* We do not return here intentionally */
    }

    ini_config_file_destroy(file_ctx);

    INIOUT(col_debug_collection(ini_config->cfg, COL_TRAVERSE_DEFAULT));

    ini_config_destroy(ini_config);

    INIOUT(printf("<==== Startup test end ====>\n"));

    return 0;
}

static int reload_test(void)
{
    int error = EOK;
    struct ini_cfgfile *file_ctx = NULL;
    struct ini_cfgfile *file_ctx_new = NULL;
    char infile[PATH_MAX];
    char outfile[PATH_MAX];
    char command[PATH_MAX * 3];
    char *srcdir;
    char *builddir;
    int changed = 0;

    INIOUT(printf("<==== Reload test ====>\n"));

    srcdir = getenv("srcdir");
    snprintf(infile, PATH_MAX, "%s/ini/ini.d/foo.conf.in",
                   (srcdir == NULL) ? "." : srcdir);
    builddir = getenv("builddir");
    snprintf(outfile, PATH_MAX, "%s/foo.conf",
                     (builddir == NULL) ? "." : builddir);

    snprintf(command, PATH_MAX * 3, "cp %s %s", infile, outfile);
    INIOUT(printf("Running command '%s'\n", command));

    error = system(command);
    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run copy command %d %d.\n",  error,
               WEXITSTATUS(error));
        return -1;
    }

    INIOUT(printf("Running chmod 660 on file '%s'\n", outfile));
    error = chmod(outfile, S_IRUSR | S_IWUSR);
    if (error) {
        error = errno;
        printf("Failed to run chmod command %d.\n",  error);
        return error;
    }

    INIOUT(printf("About to open file: %s'\n", outfile));

    /* Open config file */
    error = ini_config_file_open(outfile,
                                 INI_META_STATS,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file %s for reading. Error %d.\n",
               outfile, error);
        return error;
    }

    INIOUT(printf("About to check access to the file.\n"));

    error = ini_config_access_check(
                    file_ctx,
                    INI_ACCESS_CHECK_MODE, /* add uid & gui flags
                                            * in real case
                                            */
                    0, /* <- will be real uid in real case */
                    0, /* <- will be real gid in real case */
                    0600, /* Checkling for rw------- */
                    0);

    if (error) {
        printf("Access check failed %d!\n", error);
        ini_config_file_destroy(file_ctx);
        return EACCES;
    }

    /* ... Create config object and read configuration - not shown here.
     *     See other examples ... */

    INIOUT(printf("About to close file.\n"));

    /* Now close file but leave the context around */
    ini_config_file_close(file_ctx);

    INIOUT(printf("About to reopen file.\n"));

    /* Some time passed and we received a signal to reload... */
    error = ini_config_file_reopen(file_ctx, &file_ctx_new);
    if (error) {
        printf("Failed to re-open file for reading. Error %d.\n", error);
        ini_config_file_destroy(file_ctx);
        return error;
    }

    INIOUT(printf("About to check if the file changed.\n"));

    changed = 0;
    error = ini_config_changed(file_ctx,
                               file_ctx_new,
                               &changed);
    if (error) {
        printf("Failed to compare files. Error %d.\n",  error);
        ini_config_file_destroy(file_ctx);
        ini_config_file_destroy(file_ctx_new);
        return error;
    }

    /* Check if file changed */
    if (changed) {
        printf("File changed when it shouldn't. This is unexpected error.\n");
        ini_config_file_print(file_ctx);
        ini_config_file_print(file_ctx_new);
        ini_config_file_destroy(file_ctx);
        ini_config_file_destroy(file_ctx_new);
        return EINVAL;
    }

    INIOUT(printf("File did not change - expected. Close and force the change!.\n"));

    /* Close file */
    ini_config_file_destroy(file_ctx_new);

    INIOUT(printf("To force the change delete the file: %s\n", outfile));

    /* Emulate as if file changed */
    errno = 0;
    if (unlink(outfile)) {
        error = errno;
        printf("Failed to delete file %d.\n",  error);
        ini_config_file_destroy(file_ctx);
        return error;
    }

    sleep(1);

    snprintf(command, PATH_MAX * 3, "cp %s %s", infile, outfile);
    INIOUT(printf("Copy file again with command '%s'\n", command));

    error = system(command);
    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run copy command %d %d.\n",  error, WEXITSTATUS(error));
        ini_config_file_destroy(file_ctx);
        return -1;
    }

    INIOUT(printf("Read file again.\n"));

    /* Read again */
    file_ctx_new = NULL;
    error = ini_config_file_reopen(file_ctx, &file_ctx_new);
    if (error) {
        printf("Failed to re-open file for reading. Error %d.\n", error);
        ini_config_file_destroy(file_ctx);
        return error;
    }

    INIOUT(printf("Check if it changed.\n"));

    changed = 0;
    error = ini_config_changed(file_ctx,
                               file_ctx_new,
                               &changed);
    if (error) {
        printf("Failed to compare files. Error %d.\n",  error);
        ini_config_file_destroy(file_ctx);
        ini_config_file_destroy(file_ctx_new);
        return error;
    }

    INIOUT(printf("Changed value is %d.\n", changed));

    /* Check if file changed */
    if (!changed) {
        printf("File did not change when it should. This is an error.\n");
        ini_config_file_print(file_ctx);
        ini_config_file_print(file_ctx_new);
        ini_config_file_destroy(file_ctx);
        ini_config_file_destroy(file_ctx_new);
        return EINVAL;
    }

    INIOUT(printf("File changed!\n"));
    INIOUT(ini_config_file_print(file_ctx));
    INIOUT(ini_config_file_print(file_ctx_new));

    /* We do not need original context any more. */
    ini_config_file_destroy(file_ctx);

    /* New context is now original context */
    file_ctx = file_ctx_new;

    /* ... Create config object and read configuration - not shown here.
     *     See other examples ... */

    ini_config_file_destroy(file_ctx);

    INIOUT(printf("<==== Reload test end ====>\n"));
    return 0;
}

static int test_one_array(struct ini_cfgobj *ini_config,
                          const char *section,
                          const char *value,
                          int raw,
                          int expect,
                          const char *sep,
                          const char *message)
{
    struct value_obj *vo = NULL;
    int error = 0;
    int size = 0;
    char **strarray;
    char **strptr;
    int i;

    INIOUT(printf("%s",message));

    vo = NULL;
    error = ini_get_config_valueobj(section,
                                    value,
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n",error);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL. Section %s value %s, %s\n", section, value, raw ? "raw" : "interpreted");
        return -1;
    }

    INIOUT(value_print(value, vo));

    INIOUT(printf("Get str array with size.\n"));

    error = 0;

    if (raw) strarray = ini_get_raw_string_config_array(vo, sep, &size, &error);
    else strarray = ini_get_string_config_array(vo, sep, &size, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        return error;
    }

    /* Can be used with this cycle */
    strptr = strarray;
    while (*strptr != NULL) {
        INIOUT(printf("[%s]\n",*strptr));
        strptr++;
    }

    if (size != expect) {
        printf("Expected %d but got %d.\n", expect, size);
        ini_free_string_config_array(strarray);
        return -1;
    }

    /* Can be used with this cycle */
    INIOUT(for (i=0; i<size; i++) printf("[%s]\n", *(strarray + i)));

    ini_free_string_config_array(strarray);

    return EOK;
}

static int get_test(void)
{

    int error;
    int number;
    long number_long;
    double number_double;
    unsigned number_unsigned;
    unsigned long number_ulong;
    unsigned char logical = 0;
    char *str;
    const char *cstr;
    const char *cstrn;
    void *binary;
    int length;
    int i = 0;
    char **strarray;
    char **strptr;
    int size;
    long *array;
    double *darray;
    char **prop_array;
    int32_t val_int32;
    uint32_t val_uint32;
    int64_t val_int64;
    uint64_t val_uint64;
    struct ini_cfgfile *file_ctx = NULL;
    struct ini_cfgobj *ini_config = NULL;
    struct value_obj *vo = NULL;
    char **error_list = NULL;
    char infile[PATH_MAX];
    char *srcdir = NULL;
    int bad_val = 0;

    /* Define structure for testing arrays in the loop */
    struct a_t { char section[100];
                 char value[100];
                 int raw;
                 int expect;
                 char sep[10];
                 char message[100]; } array_test[NUM_TESTS] =
            {
                { "services",
                  "activeServices",
                  0,
                  4,
                  ",:;",
                  "Make sure we parse string array properly\n"},

                { "services",
                  "activeServices",
                  1,
                  4,
                  ",:;",
                  "Make sure we parse raw string array properly\n"},

                { "domains",
                  "domainsOrder",
                  0,
                  3,
                  ",:;",
                  "Parse string array with comma at the end\n"},

                { "domains",
                  "domainsOrder",
                  1,
                  8,
                  ",:;",
                  "Parse raw string array with comma at the end\n"},

                { "domains",
                  "badarray",
                  0,
                  0,
                  ",:;",
                  "Bad array should have no tokens\n"},

                { "domains",
                  "badarray",
                  1,
                  6,
                  ",:;",
                  "Raw bad array should have right number of tokens.\n"},

                { "domains",
                  "somearray",
                  0,
                  0,
                  ",:;",
                  "Bad array should have no tokens\n"},

                { "domains",
                  "somearray",
                  1,
                  2,
                  ",:;",
                  "Raw bad array should have right number of tokens.\n"},

                { "domains",
                  "someotherarray",
                  0,
                  0,
                  ",:;",
                  "Bad array should have no tokens\n"},

                { "domains",
                  "someotherarray",
                  1,
                  3,
                  ",:;",
                  "Raw bad array should have right number of tokens.\n"},

                { "domains",
                  "justdelim",
                  0,
                  0,
                  ",:;",
                  "Bad array should have no tokens\n"},

                { "domains",
                  "justdelim",
                  1,
                  5,
                  ",:;",
                  "Delimeters only should have right number of tokens.\n"},

                { "domains",
                  "yetanother",
                  0,
                  0,
                  ",:;",
                  "Empty array should have no tokens.\n"},

                { "domains",
                  "yetanother",
                  1,
                  0,
                  ",:;",
                  "Empty raw array should have no token.\n"}
            };

    /*****************************************************/

    INIOUT(printf("\n\n<==== GET TEST START =====>\n"));
    INIOUT(printf("Creating configuration object\n"));

    /* Create config collection */
    error = ini_config_create(&ini_config);
    if (error) {
        printf("Failed to create collection. Error %d.\n", error);
        return error;
    }

    srcdir = getenv("srcdir");
    snprintf(infile, PATH_MAX, "%s/ini/ini.d/real.conf",
             (srcdir == NULL) ? "." : srcdir);

    INIOUT(printf("Reading file %s\n", infile));

    error = ini_config_file_open(infile,
                                 0,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file for reading. Error %d.\n",  error);
        ini_config_destroy(ini_config);
        return error;
    }

    error = ini_config_parse(file_ctx,
                             INI_STOP_ON_NONE,
                             /* Merge section but allow duplicates */
                             INI_MS_MERGE |
                             INI_MV1S_ALLOW |
                             INI_MV2S_ALLOW,
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
        }
        /* We do not return here intentionally */
    }

    ini_config_file_destroy(file_ctx);

    INIOUT(printf("Negtive test - trying to get non"
                  " existing key-value pair.\n"));

    /* Negative test */
    vo = NULL;
    error = ini_get_config_valueobj("monitor1",
                                    "description1",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Values should not be found */
    if (vo != NULL) {
        printf("Expected NULL but got something else!\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    /* Another negative test but section exists this time */
    vo = NULL;
    error = ini_get_config_valueobj("monitor",
                                    "description1",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Valueobj should not be found */
    if(vo != NULL) {
        printf("Expected NULL but got something else!\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Trying to get a value.\n"));

    /* Positive test */
    vo = NULL;
    error = ini_get_config_valueobj("monitor",
                                    "description",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected value but got NULL!\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("description", vo));

    INIOUT(printf("Get values as string without duplication"
                  " from the NULL valueobj.\n"));

    /* Get a string without duplicication */
    /* Negative test */
    cstrn = ini_get_const_string_config_value(NULL, NULL);
    if (cstrn != NULL) {
        printf("Expected error got success.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Get value as string without duplication"
                  "from the correct value object.\n"));

    /* Now get string from the right value object */
    error = 0;
    cstr = ini_get_const_string_config_value(vo, &error);
    if (error) {
        printf("Expected success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    INIOUT(printf("Value: [%s]\n", cstr));

    /* Same thing but create a dup */

    INIOUT(printf("Get value as string with duplication"
                  " from correct value object.\n"));

    error = 0;
    str = ini_get_string_config_value(vo, &error);
    if (error) {
        printf("Expected success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    INIOUT(printf("Value: [%s]\n", str));
    free(str);


    /* Get a badly formated number */
    INIOUT(printf("Convert value to number with strict conversion.\n"));

    vo = NULL;
    error = ini_get_config_valueobj("monitor",
                                    "bad_number",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected value but got something NULL!\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("bad_number", vo));

    /* Now try to get value in different ways */
    error = 0;
    number = ini_get_int_config_value(vo, 1, 10, &error);
    if (error) {
        /* We expected error in this case */
        INIOUT(printf("Expected error.\n"));
        if(number != 10) {
            printf("It failed to set default value.\n");
            ini_config_destroy(ini_config);
            return -1;
        }
    }
    else {
        printf("Expected error got success.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Convert value to number without strict conversion.\n"));

    error = 0;
    number = ini_get_int_config_value(vo, 0, 10, &error);
    if (error) {
        printf("Did not expect error.\n");
        ini_config_destroy(ini_config);
        return error;
    }

    if (number != 5) {
        printf("We expected that the conversion will return 5.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    /* Get real integer */

    INIOUT(printf("Fetch another value from section \"domains/LOCAL\""
                  " named \"enumerate\".\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/LOCAL",
                                    "enumerate",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Convert value to integer.\n"));

    /* Take number out of it */
    error = 0;
    number = ini_get_int_config_value(vo, 1, 100, &error);
    if (error) {
        printf("Did not expect error. Got %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* It is 3 in the file */
    if (number != 3) {
        printf("We expected that the conversion will return 3.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Expected 3 got %d\n", number));

    INIOUT(printf("Convert value to long.\n"));

    /* Take number out of it */
    error = 0;
    number_long = ini_get_long_config_value(vo, 1, 100, &error);
    if (error) {
        printf("Did not expect error. Got %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* It is 3 in the file */
    if (number_long != 3) {
        printf("We expected that the conversion will return 3.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Expected 3 got %ld\n", number_long));

    INIOUT(printf("Convert value to unsigned.\n"));

    /* Take number out of it */
    error = 0;
    number_unsigned = ini_get_unsigned_config_value(vo, 1, 100, &error);
    if (error) {
        printf("Did not expect error. Got %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* It is 3 in the file */
    if (number_unsigned != 3) {
        printf("We expected that the conversion will return 3.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Expected 3 got %d\n", number_unsigned));

    INIOUT(printf("Convert value to unsigned long.\n"));

    /* Take number out of it */
    error = 0;
    number_ulong = ini_get_ulong_config_value(vo, 1, 100, &error);
    if (error) {
        printf("Did not expect error. Got %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* It is 3 in the file */
    if (number_ulong != 3) {
        printf("We expected that the conversion will return 3.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Expected 3 got %lu\n", number_ulong));

    INIOUT(printf("Convert value to double.\n"));

    /* Take number out of it */
    error = 0;
    number_double = ini_get_double_config_value(vo, 1, 100., &error);
    if (error) {
        printf("Did not expect error. Got %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* It is 3 in the file */
    if (number_double != 3.) {
        printf("We expected that the conversion will return 3.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Expected 3 got %e\n", number_double));

    INIOUT(printf("Convert value to bool.\n"));

    /* Take number out of it */
    error = 0;
    logical = ini_get_bool_config_value(vo, 1, &error);
    if (!error) {
        printf("Expect error. Got success. Value %d\n", (int) logical);
        ini_config_destroy(ini_config);
        return -1;
    }

    /* Get real bool values and convert it */
    INIOUT(printf("Get real bool value \"legacy\" and convert it.\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/LOCAL",
                                    "legacy",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n",error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Convert values to bool.\n"));

    error = 0;
    logical = ini_get_bool_config_value(vo, 1, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    if (logical) {
        printf("Expected false but got true - bad.\n");
        return -1;
    }

    INIOUT(printf("In the files it is FALSE so we got false.\n"));

    INIOUT(printf("Get binary value\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "binary_test",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("binary_test", vo));

    error = 0;
    binary = ini_get_bin_config_value(vo, &length, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    INIOUT(printf("Binary value (expect 123) = "));
    INIOUT(for (i = 0; i < length; i++) {
                printf("%d",*((unsigned char*)(binary) + i));
                if (*((unsigned char*)(binary) + i) != (i + 1)) bad_val = 1;
           });
    INIOUT(printf("\n"));

    ini_free_bin_config_value(binary);

    if (bad_val) {
        printf("Unexpected binary value.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Get another binary value\n"));

    bad_val = 0;
    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "binary_test_two",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("binary_test_two", vo));

    error = 0;
    binary = ini_get_bin_config_value(vo, &length, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    INIOUT(printf("Binary value (expect abc) = "));
    INIOUT(for (i = 0; i < length; i++) {
                printf("%x",*((unsigned char*)(binary) + i));
                if (*((unsigned char*)(binary) + i) - 10 != i) bad_val = 1;
           });
    INIOUT(printf("\n"));

    ini_free_bin_config_value(binary);

    if (bad_val) {
        printf("Unexpected binary value.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("Get string array value\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains",
                                    "domainsorder",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n",error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("domainsorder", vo));

    INIOUT(printf("Get str array without size.\n"));

    error = 0;
    strarray = ini_get_string_config_array(vo, ",", NULL, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Can be used with this cycle */
    strptr = strarray;
    while (*strptr != NULL) {
        INIOUT(printf("[%s]\n",*strptr));
        strptr++;
    }

    ini_free_string_config_array(strarray);

    INIOUT(printf("Get raw str array without size.\n"));

    error = 0;
    strarray = ini_get_raw_string_config_array(vo, ",", NULL, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Can be used with this cycle */
    strptr = strarray;
    while (*strptr != NULL) {
        INIOUT(printf("[%s]\n",*strptr));
        strptr++;
    }

    ini_free_string_config_array(strarray);

    INIOUT(printf("Get str array with size.\n"));

    error = 0;
    size = 0;
    strarray = ini_get_string_config_array(vo, ",", &size, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Can be used with this cycle */
    INIOUT(for (i=0;i<size;i++) printf("[%s]\n",*(strarray + i)));

    ini_free_string_config_array(strarray);

    INIOUT(printf("Get raw str array with size.\n"));

    error = 0;
    size = 0;
    strarray = ini_get_raw_string_config_array(vo, ",", &size, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Can be used with this cycle */
    INIOUT(for (i=0;i<size;i++) printf("[%s]\n",*(strarray + i)));

    ini_free_string_config_array(strarray);

    /**********************************************************/

    INIOUT(printf("Get bad string array \n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains",
                                    "badarray",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n",error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("badarray", vo));

    INIOUT(printf("Get bad str array without size.\n"));

    error = 0;
    strarray = ini_get_string_config_array(vo, ",", NULL, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Can be used with this cycle */
    strptr = strarray;
    while (*strptr != NULL) {
        INIOUT(printf("[%s]\n",*strptr));
        strptr++;
    }

    ini_free_string_config_array(strarray);

    /**********************************************************/

    for (i = 0; i< NUM_TESTS; i++) {
        error = test_one_array(ini_config,
                               array_test[i].section,
                               array_test[i].value,
                               array_test[i].raw,
                               array_test[i].expect,
                               array_test[i].sep,
                               array_test[i].message);
        if (error) {
            /* Message is printed inside function */
            ini_config_destroy(ini_config);
            return error;
        }
    }

    /**********************************************************/

    INIOUT(printf("Get long array value\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "long_array",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("long_array", vo));

    error = 0;
    size = 0; /* Here size is not optional!!! */
    array = ini_get_long_config_array(vo, &size, &error);
    if(error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Can be used with this cycle */
    INIOUT(for (i=0;i<size;i++) printf("%ld\n", *(array + i)));

    ini_free_long_config_array(array);

    INIOUT(printf("Get double array value\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "double_array",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Values should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("double_array", vo));

    error = 0;
    size = 0; /* Here size is not optional!!! */
    darray = ini_get_double_config_array(vo, &size, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Can be used with this cycle */
    INIOUT(for (i=0;i<size;i++) printf("%.4f\n", darray[i]));

    ini_free_double_config_array(darray);

    INIOUT(printf("\n\nSection list - no size\n"));

    /* Do not care about the error or size */
    prop_array = ini_get_section_list(ini_config, NULL, NULL);
    if (prop_array == NULL) {
        printf("Expect success got error.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    i = 0;
    INIOUT(while (prop_array[i]) {
               printf("Section: [%s]\n", prop_array[i]);
               i++;
           });

    ini_free_section_list(prop_array);

    INIOUT(printf("\n\nSection list - with size\n"));

    /* Do not care about the error or size */
    prop_array = ini_get_section_list(ini_config, &size, NULL);
    if (prop_array == NULL) {
        printf("Expect success got error.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(for (i=0;i<size;i++) printf("Section: [%s]\n", prop_array[i]));
    ini_free_section_list(prop_array);

    INIOUT(printf("\n\nAttributes in the section - with size and error\n"));

    /* Do not care about the error or size */
    prop_array = ini_get_attribute_list(ini_config,
                                    "domains/EXAMPLE.COM",
                                    &size,
                                    &error);
    if (prop_array == NULL) {
        printf("Expect success got error.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(for (i=0;i<size;i++) printf("Attribute: [%s]\n", prop_array[i]));
    ini_free_attribute_list(prop_array);


    /***************************************/
    /* Test special types                  */
    /***************************************/
    INIOUT(printf("Test int32_t\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "int32_t",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("int32_t", vo));

    error = 0;
    val_int32 = ini_get_int32_config_value(vo, 1, 0, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    INIOUT(printf("Value: %d\n", val_int32));

    /***************************************/

    INIOUT(printf("Test uint32_t\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "uint32_t",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Valu should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("uint32_t", vo));

    error = 0;
    val_uint32 = ini_get_uint32_config_value(vo, 1, 0, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
   }

    INIOUT(printf("Value: %u\n", val_uint32));

    /***************************************/

    INIOUT(printf("Test int64_t\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "int64_t",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("int64_t", vo));

    error = 0;
    val_int64 = ini_get_int64_config_value(vo, 1, 0, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    INIOUT(printf("Value: %lld\n", (long long)val_int64));

    /***************************************/

    INIOUT(printf("Test uint32_t\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "uint64_t",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if (error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("uint64_t", vo));

    error = 0;
    val_uint64 = ini_get_uint64_config_value(vo, 1, 0, &error);
    if (error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    INIOUT(printf("Value: %llu\n", (unsigned long long)val_uint64));

    /***************************************/

    INIOUT(printf("Get empty array value object\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "empty_value",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("empty_value", vo));

    error = 0;
    size = 0; /* Here size is not optional!!! */
    strarray = ini_get_string_config_array(vo, ",", &size, &error);
    if(error) {
        printf("Expect success got error %d.\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    if (size != 0) {
        for (i=0; i<size; i++) printf("%s\n", *(strarray + i));
        printf("Expected size=0, got size=%d\n", size);
        ini_free_string_config_array(strarray);
        ini_config_destroy(ini_config);
        return -1;
    }

    ini_free_string_config_array(strarray);

    /***************************************/

    INIOUT(printf("\nGet last value\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "server",
                                    ini_config,
                                    INI_GET_LAST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("server", vo));

    /***************************************/

    INIOUT(printf("\nGet sequence of the multi-value keys\n"));

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "server",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n", error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(value_print("server", vo));


    do {

        vo = NULL;
        error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                        "server",
                                        ini_config,
                                        INI_GET_NEXT_VALUE,
                                    &vo);
        if(error) {
            printf("Expected success but got error! %d\n", error);
            ini_config_destroy(ini_config);
            return error;
        }

        if (vo == NULL) break;

        INIOUT(value_print("server", vo));
    }
    while(1);

    /***************************************/

    INIOUT(printf("\nGet multi-value keys without prefetching\n"));

    do {

        vo = NULL;
        error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                        "server",
                                        ini_config,
                                        INI_GET_NEXT_VALUE,
                                    &vo);
        if(error) {
            printf("Expected success but got error! %d\n", error);
            ini_config_destroy(ini_config);
            return error;
        }

        if (vo == NULL) break;

        INIOUT(value_print("server", vo));
    }
    while(1);

    /***************************************/

    INIOUT(printf("\nGet multi-value keys with key interrupt\n"));

    i = 0;

    vo = NULL;
    do {

        vo = NULL;
        error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                        "server",
                                        ini_config,
                                        INI_GET_NEXT_VALUE,
                                        &vo);
        if(error) {
            printf("Expected success but got error! %d\n", error);
            ini_config_destroy(ini_config);
            return error;
        }

        if (vo == NULL) break;

        INIOUT(value_print("server", vo));
        i++;

        if (i==2) {
            vo = NULL;
            error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                            "empty_value",
                                            ini_config,
                                            INI_GET_NEXT_VALUE,
                                            &vo);
            if(error) {
                printf("Expected success but got error! %d\n", error);
                ini_config_destroy(ini_config);
                return error;
            }
        }
    }
    while(1);

    if (i != 6) {
        printf("Expected 6 iterations got %d\n", i);
        ini_config_destroy(ini_config);
        return -1;
    }

    /***************************************/

    INIOUT(printf("\nGet multi-value keys with section interrupt\n"));

    i = 0;

    vo = NULL;
    do {

        vo = NULL;
        error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                        "server",
                                        ini_config,
                                        INI_GET_NEXT_VALUE,
                                    &vo);
        if(error) {
            printf("Expected success but got error! %d\n", error);
            ini_config_destroy(ini_config);
            return error;
        }

        if (vo == NULL) break;

        INIOUT(value_print("server", vo));
        i++;

        if (i==2) {
            vo = NULL;
            error = ini_get_config_valueobj("domains",
                                            "badarray",
                                            ini_config,
                                            INI_GET_NEXT_VALUE,
                                        &vo);
            if(error) {
                printf("Expected success but got error! %d\n", error);
                ini_config_destroy(ini_config);
                return error;
            }
        }
    }
    while(1);

    if (i != 6) {
        printf("Expected 6 iterations got %d\n", i);
        ini_config_destroy(ini_config);
        return -1;
    }

    ini_config_destroy(ini_config);

    INIOUT(printf("\n<==== GET TEST END =====>\n\n"));
    return EOK;
}

static int space_test(void)
{

    int error;
    struct ini_cfgfile *file_ctx = NULL;
    struct ini_cfgobj *ini_config = NULL;
    char **error_list = NULL;
    char infile[PATH_MAX];
    char *srcdir = NULL;
    unsigned errnum;
    int i;
    uint32_t flags[] = { INI_PARSE_NOWRAP,
                         INI_PARSE_NOWRAP |
                         INI_PARSE_NOSPACE,
                         INI_PARSE_NOWRAP |
                         INI_PARSE_NOTAB,
                         INI_PARSE_NOWRAP |
                         INI_PARSE_NOSPACE |
                         INI_PARSE_NOTAB };

    INIOUT(printf("\n\n<==== SPACE TEST START =====>\n"));

    srcdir = getenv("srcdir");
    snprintf(infile, PATH_MAX, "%s/ini/ini.d/space.conf",
             (srcdir == NULL) ? "." : srcdir);


    for (i = 0; i < 4; i++ ) {

        INIOUT(printf("Reading file %s\n", infile));
        error = ini_config_file_open(infile,
                                        0,
                                        &file_ctx);
        if (error) {
            printf("Failed to open file for reading. Error %d.\n",  error);
            return error;
        }

        INIOUT(printf("Creating configuration object\n"));
        error = ini_config_create(&ini_config);
        if (error) {
            printf("Failed to create object. Error %d.\n", error);
            ini_config_file_destroy(file_ctx);
            return error;
        }
        INIOUT(printf("Parsing\n"));
        error = ini_config_parse(file_ctx,
                                 INI_STOP_ON_NONE,
                                 0,
                                 flags[i],
                                 ini_config);
        if (error) {
            INIOUT(printf("Failed to parse configuration. Error %d.\n", error));

            errnum = ini_config_error_count(ini_config);
            if (errnum) {
                INIOUT(printf("Errors detected while parsing: %s\n",
                       ini_config_get_filename(file_ctx)));
                ini_config_get_errors(ini_config, &error_list);
                INIOUT(ini_config_print_errors(stdout, error_list));
                ini_config_free_errors(error_list);
            }
            if (((i == 0) && (errnum != 0)) ||
                ((i == 1) && (errnum != 3)) ||
                ((i == 2) && (errnum != 3)) ||
                ((i == 3) && (errnum != 4))) {
                printf("Failed to open file for reading. Error %d.\n",  error);
                ini_config_file_destroy(file_ctx);
                ini_config_destroy(ini_config);
                return -1;
            }
            /* We do not return here intentionally */
        }

        INIOUT(col_debug_collection(ini_config->cfg, COL_TRAVERSE_DEFAULT));
        ini_config_destroy(ini_config);
        ini_config_file_destroy(file_ctx);
    }

    INIOUT(printf("\n<==== SPACE TEST END =====>\n\n"));
    return EOK;
}


static int trim_test(void)
{
    int error;
    struct ini_cfgfile *file_ctx = NULL;
    struct ini_cfgobj *ini_config = NULL;
    char **error_list = NULL;
    char infile[PATH_MAX];
    char *srcdir = NULL;
    const char *value;
    struct value_obj *vo = NULL;

    INIOUT(printf("\n\n<==== TRIM TEST START =====>\n"));

    srcdir = getenv("srcdir");
    snprintf(infile, PATH_MAX, "%s/ini/ini.d/real.conf",
             (srcdir == NULL) ? "." : srcdir);


    INIOUT(printf("Reading file %s\n", infile));
    error = ini_config_file_open(infile,
                                 0,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file for reading. Error %d.\n",  error);
        return error;
    }

    INIOUT(printf("Creating configuration object\n"));
    error = ini_config_create(&ini_config);
    if (error) {
        printf("Failed to create object. Error %d.\n", error);
        ini_config_file_destroy(file_ctx);
        return error;
    }
    INIOUT(printf("Parsing\n"));
    error = ini_config_parse(file_ctx,
                             INI_STOP_ON_NONE,
                             0,
                             0,
                             ini_config);
    if (error) {
        INIOUT(printf("Failed to parse configuration. "
                      "Error %d.\n", error));

        if (ini_config_error_count(ini_config)) {
            INIOUT(printf("Errors detected while parsing: %s\n",
                   ini_config_get_filename(file_ctx)));
            ini_config_get_errors(ini_config, &error_list);
            INIOUT(ini_config_print_errors(stdout, error_list));
            ini_config_free_errors(error_list);
        }
        ini_config_file_destroy(file_ctx);
        return error;
    }

    INIOUT(col_debug_collection(ini_config->cfg, COL_TRAVERSE_DEFAULT));
    ini_config_file_destroy(file_ctx);

    vo = NULL;
    error = ini_get_config_valueobj("domains/EXAMPLE.COM",
                                    "description",
                                    ini_config,
                                    INI_GET_FIRST_VALUE,
                                    &vo);
    if(error) {
        printf("Expected success but got error! %d\n",error);
        ini_config_destroy(ini_config);
        return error;
    }

    /* Value should be found */
    if (vo == NULL) {
        printf("Expected success but got NULL.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    value = ini_get_const_string_config_value(vo, NULL);

    if (value == NULL) {
        printf("No value.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    if(value[strlen(value) - 1] == ' ') {
        printf("Trailing space is not trimmed.\n");
        ini_config_destroy(ini_config);
        return -1;
    }

    INIOUT(printf("[%s]\n", value));

    ini_config_destroy(ini_config);

    INIOUT(printf("\n<==== TRIM TEST END =====>\n\n"));
    return EOK;
}

static int comment_test(void)
{
    int error;
    struct ini_cfgfile *file_ctx = NULL;
    struct ini_cfgobj *ini_config = NULL;
    char **error_list = NULL;
    char infile[PATH_MAX];
    char *srcdir = NULL;
    int err_count = 0;

    INIOUT(printf("\n\n<==== COMMENT TEST START =====>\n"));

    srcdir = getenv("srcdir");
    snprintf(infile, PATH_MAX, "%s/ini/ini.d/comment.conf",
             (srcdir == NULL) ? "." : srcdir);


    INIOUT(printf("Reading file %s\n", infile));
    error = ini_config_file_open(infile,
                                 0,
                                 &file_ctx);
    if (error) {
        printf("Failed to open file for reading. Error %d.\n",  error);
        return error;
    }

    INIOUT(printf("Creating configuration object\n"));
    error = ini_config_create(&ini_config);
    if (error) {
        printf("Failed to create object. Error %d.\n", error);
        ini_config_file_destroy(file_ctx);
        return error;
    }
    INIOUT(printf("Parsing\n"));
    error = ini_config_parse(file_ctx,
                             INI_STOP_ON_NONE,
                             0,
                             0,
                             ini_config);
    if (error) {
        INIOUT(printf("Failed to parse configuration. "
                      "Error %d.\n", error));
        err_count = ini_config_error_count(ini_config);
        if (err_count) {
            INIOUT(printf("Errors detected while parsing: %s\n",
                   ini_config_get_filename(file_ctx)));
            ini_config_get_errors(ini_config, &error_list);
            INIOUT(ini_config_print_errors(stdout, error_list));
            ini_config_free_errors(error_list);
        }
    }

    INIOUT(col_debug_collection(ini_config->cfg, COL_TRAVERSE_DEFAULT));
    ini_config_file_destroy(file_ctx);
    ini_config_destroy(ini_config);

    if(err_count != 4) {
        printf("Expected 4 errors got: %d\n", err_count);
        return 1;
    }

    INIOUT(printf("\n<==== COMMENT TEST END =====>\n\n"));
    return EOK;
}

static void create_boms(void)
{
    FILE *f;

    f = fopen("bom2be","wb");
    fprintf(f,"%c%c", 0xFE, 0xFF);
    fclose(f);
    f = fopen("bom2le","wb");
    fprintf(f,"%c%c", 0xFF, 0xFE);
    fclose(f);
    f = fopen("bom4be","wb");
    fprintf(f,"%c%c%c%c", 0x00, 0x00, 0xFE, 0xFF);
    fclose(f);
    f = fopen("bom4le","wb");
    fprintf(f,"%c%c%c%c", 0xFF, 0xFE, 0x00, 0x00);
    fclose(f);
    f = fopen("bom3","wb");
    fprintf(f,"%c%c%c", 0xEF, 0xBB, 0xBF);
    fclose(f);
}


/* Main function of the unit test */
int main(int argc, char *argv[])
{
    int error = 0;
    test_fn tests[] = { read_save_test,
                        read_again_test,
                        read_mem_test,
                        merge_values_test,
                        merge_section_test,
                        merge_file_test,
                        startup_test,
                        reload_test,
                        get_test,
                        space_test,
                        trim_test,
                        comment_test,
                        NULL };
    test_fn t;
    int i = 0;
    char *var;

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = 1;
    else {
        var = getenv("COMMON_TEST_VERBOSE");
        if (var) verbose = 1;
    }

    /* Create boms in case we want to create more test files */
    create_boms();

    INIOUT(printf("Start\n"));

    while ((t = tests[i++])) {
        error = t();
        fflush(NULL);
        if (error) {
            INIOUT(printf("Failed with error %d!\n", error));
            return error;
        }
    }

    INIOUT(printf("Success!\n"));

    return 0;
}
