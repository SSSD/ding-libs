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
#include "ini_configmod.h"
#include "path_utils.h"

int verbose = 0;

#define WRAP_SIZE 80

#define INIOUT(foo) \
    do { \
        if (verbose) { printf("%30s(%4d): ",__FUNCTION__,__LINE__); foo; } \
    } while(0)

typedef int (*test_fn)(void);

static void print_configuration(struct ini_cfgobj *in_cfg,
                         FILE *file)
{
    int error;
    struct simplebuffer *sbobj = NULL;
    uint32_t left = 0;

    error = simplebuffer_alloc(&sbobj);
    if (error) {
        printf("Failed to allocate buffer. Error %d.\n", error);
        return;
    }

    error = ini_config_serialize(in_cfg, sbobj);
    if (error) {
        printf("Failed to serialize. Error %d.\n", error);
        simplebuffer_free(sbobj);
        return;
    }

    /* Save */
    left = simplebuffer_get_len(sbobj);
    while (left > 0) {
        error = simplebuffer_write(fileno(file), sbobj, &left);
        if (error) {
            printf("Failed to write back the configuration %d.\n", error);
            simplebuffer_free(sbobj);
            return;
        }
    }

    simplebuffer_free(sbobj);
    return;
}



/* Basic test */
static int basic_test(void)
{
    int error = EOK;
    char indir[PATH_MAX];
    char srcname[PATH_MAX];
    char resname[PATH_MAX];
    char command[PATH_MAX * 3];
    char *builddir = NULL;
    char *srcdir = NULL;
    struct ini_cfgobj *in_cfg = NULL;
    char bin1[] = { 1, 2, 3 };
    char bin2[] = { 10, 11, 12 };
    FILE *file = NULL;
    const char *comment1[] = { "// This is a real file with some comments",
                               "" };
    const char *comment2[] = { "",
                               "/**/" };
    const char *comment3[] = { "",
                               "/* Service section defines",
                               " * which service are allowed.",
                               " */           "
                             };
    const char *sec_com[] = { "" };
    const char *const_str_arr[] = { "dp", "nss", "pam", "info" };
    char **str_arr = (char **)(intptr_t)const_str_arr;

    INIOUT(printf("<==== Start ====>\n"));

    srcdir = getenv("srcdir");
    builddir = getenv("builddir");

    snprintf(indir, PATH_MAX, "%s/ini/ini.d",
                    (srcdir == NULL) ? "." : srcdir);

    snprintf(srcname, PATH_MAX, "%s/ini/ini.d/real.conf",
                    (srcdir == NULL) ? "." : srcdir);

    snprintf(resname, PATH_MAX, "%s/real.conf.manual",
                    (builddir == NULL) ? "." : builddir);

    /* Create config collection */
    error = ini_config_create(&in_cfg);
    if (error) {
        INIOUT(printf("Failed to create collection. Error %d.\n", error));
        return error;
    }

    if ((error = ini_config_add_section(
                 in_cfg,
                 "config",
                 comment1,
                 2,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "config",
                 "version",
                 "0.1",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "monitor",
                 comment2,
                 2,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "monitor",
                 "description",
                 "Monitor Configuration",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_int_value(
                 in_cfg,
                 "monitor",
                 "sbusTimeout",
                 10,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "monitor",
                 "sbusAddress",
                 "unix:path=/var/lib/sss/pipes/private/dbus",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_unsigned_value(
                 in_cfg,
                 "monitor",
                 "servicePingTime",
                 10,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "monitor",
                 "bad_number",
                 "5a",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "services",
                 comment3,
                 4,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services",
                 "description",
                 "Local service configuration",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_arr_value(
                 in_cfg,
                 "services",
                 "activeServices",
                 str_arr,
                 4,
                 ',',
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "services/dp",
                 sec_com,
                 1,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/dp",
                 "description",
                 "Data Provider Configuration",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/dp",
                 "command",
                 "/usr/libexec/sssd/sssd_dp",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "services/nss",
                 sec_com,
                 1,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/nss",
                 "description",
                 "NSS Responder Configuration",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/nss",
                 "unixSocket",
                 "/var/lib/sss/pipes/nss",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/nss",
                 "command",
                 "/usr/libexec/sssd/sssd_nss",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "services/pam",
                 sec_com,
                 1,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/pam",
                 "description",
                 "PAM Responder Configuration",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/pam",
                 "unixSocket",
                 "/var/lib/sss/pipes/pam",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/pam",
                 "command",
                 "/usr/libexec/sssd/sssd_pam",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_FRONT,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "services/info",
                 sec_com,
                 1,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/info",
                 "description",
                 "InfoPipe Configuration",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "services/info",
                 "command",
                 "./sbin/sssd_info",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "domains",
                 sec_com,
                 1,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains",
                 "domainsOrder",
                 " , LOCAL,          ,  EXAMPLE.COM"
                 ", ,     SOMEOTHER.COM    ,  ,",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains",
                 "badarray",
                 "   ,   ,    ,   ,   ,",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains",
                 "somearray",
                 ",",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains",
                 "someotherarray",
                 ", ;",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains",
                 "justdelim",
                 ":;,;",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains",
                 "yetanother",
                 "",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "domains/LOCAL",
                 sec_com,
                 1,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/LOCAL",
                 "description",
                 "Reserved domain for local configurations",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/LOCAL",
                 "legacy",
                 "FALSE",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_long_value(
                 in_cfg,
                 "domains/LOCAL",
                 "enumerate",
                 3,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_section(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 sec_com,
                 1,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "description",
                 "Example domain served by IPA ",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "provider",
                 "ipa",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "server",
                 "ipaserver1.example.com",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "server",
                 "ipabackupserver.example.com",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "legacy",
                 "FALSE",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "server",
                 "otheripabackupserver.example.com",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_int64_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "enumerate",
                 0,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_bin_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "binary_test",
                 bin1,
                 3,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_bin_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "binary_test_two",
                 bin2,
                 3,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "long_array",
                 "1  2; 4' ;8p .16/ 32?",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "double_array",
                 "1.1  2.222222; .4' . ;8p .16/ -32?",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "server",
                 "yetanotheripabackupserver.example.com",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "empty_value",
                 "",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "space_value",
                 "\" \"",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_int32_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "int32_t",
                 -1000000000,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_uint32_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "uint32_t",
                 3000000000u,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_int64_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "int64_t",
                 -1000000000123,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_uint64_value(
                 in_cfg,
                 "domains/EXAMPLE.COM",
                 "uint64_t",
                 9223375036854775931u,
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_END,
                 NULL,
                 0,
                 INI_VA_NOCHECK))) {
        INIOUT(printf("Failed to create configuration. Error %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    file = fopen(resname, "w");
    print_configuration(in_cfg, file);
    fclose(file);
    ini_config_destroy(in_cfg);

    snprintf(command, PATH_MAX * 3, "diff -wi %s %s", srcname, resname);
    error = system(command);
    INIOUT(printf("Comparison of %s %s returned: %d\n",
                  srcname, resname, error));

    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run diff command %d %d.\n",  error,
               WEXITSTATUS(error));
        return -1;
    }


    INIOUT(printf("<==== End ====>\n"));

    return EOK;
}


static void make_results(const char *path)
{
    FILE *file = NULL;

    file = fopen(path, "w");

    fprintf(file, "// This is a test\n"
                  "[one]\n"
                  "key1 = value1\n"
                  "key1 = value1a\n"
                  "key1 = value1a_bis\n"
                  "// This is a test\n"
                  "key1 = value1b\n"
                  "key2 = value2\n"
                  "key3 = value3\n");
    fclose(file);
}


/* Test duplicates */
static int dup_test(void)
{
    int error = EOK;
    char srcname[PATH_MAX];
    char resname[PATH_MAX];
    char command[PATH_MAX * 3];
    char *builddir = NULL;
    struct ini_cfgobj *in_cfg = NULL;
    FILE *file = NULL;
    const char *comment[] = { "// This is a test", NULL };

    INIOUT(printf("<==== Start ====>\n"));

    builddir = getenv("builddir");

    snprintf(srcname, PATH_MAX, "%s/modtest.conf.exp",
                    (builddir == NULL) ? "." : builddir);

    snprintf(resname, PATH_MAX, "%s/modtest.conf.real",
                    (builddir == NULL) ? "." : builddir);

    make_results(srcname);

    /* Create config collection */
    error = ini_config_create(&in_cfg);
    if (error) {
        INIOUT(printf("Failed to create collection. Error %d.\n", error));
        return error;
    }

    if ((error = ini_config_add_section(
                 in_cfg,
                 "one",
                 NULL,
                 0,
                 COL_DSP_END,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "one",
                 "key2",
                 "value2",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_FRONT,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "one",
                 "key1",
                 "value1a",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_BEFORE,
                 "key2",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "one",
                 "key3",
                 "value3",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_AFTER,
                 "key2",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "one",
                 "key1",
                 "value1",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_FIRSTDUP,
                 "key1",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "one",
                 "key1",
                 "value1b",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_LASTDUP,
                 "key1",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "one",
                 "key1",
                 "value1c",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_LASTDUP,
                 "key1",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "one",
                 "key1",
                 "value1a_bis",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_NDUPNS,
                 "key1",
                 2,
                 INI_VA_NOCHECK))) {
        INIOUT(printf("Failed to create configuration. Error %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    /* Try to add another section "one" */
    if (EEXIST != ini_config_add_section(
                 in_cfg,
                 "one",
                 NULL,
                 0,
                 COL_DSP_END,
                 NULL,
                 0)) {
        INIOUT(printf("Expected error. Got %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    if ((error = ini_config_comment_section(in_cfg,
                                            "one",
                                            comment,
                                            1))) {
        INIOUT(printf("Failed to add a comment %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    if ((error = ini_config_add_section(
                 in_cfg,
                 "two",
                 NULL,
                 0,
                 COL_DSP_FRONT,
                 NULL,
                 0)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "two",
                 "key2",
                 "value2",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_FRONT,
                 NULL,
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "two",
                 "key1",
                 "value1a",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_BEFORE,
                 "key2",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "two",
                 "key3",
                 "value3",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_AFTER,
                 "key2",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "two",
                 "key1",
                 "value1",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_FIRSTDUP,
                 "key1",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "two",
                 "key1",
                 "value1b",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_LASTDUP,
                 "key1",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "two",
                 "key1",
                 "value1c",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_LASTDUP,
                 "key1",
                 0,
                 INI_VA_NOCHECK)) ||
        (error = ini_config_add_str_value(
                 in_cfg,
                 "two",
                 "key1",
                 "value1a_bis",
                 NULL,
                 0,
                 WRAP_SIZE,
                 COL_DSP_NDUPNS,
                 "key1",
                 2,
                 INI_VA_NOCHECK))) {
        INIOUT(printf("Failed to add another section. Error %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    /* Rename section */
    if ((error = ini_config_rename_section(in_cfg,
                                           "two",
                                           "three"))) {
        INIOUT(printf("Failed to rename a section %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    /* Delect section that is before section "one" */
    if ((error = ini_config_delete_section_by_position(in_cfg,
                                                       COL_DSP_BEFORE,
                                                       "one",
                                                       0))) {
        INIOUT(printf("Failed to delete a section %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    /* Update comment */
    if ((error = ini_config_update_comment(in_cfg,
                                           "one",
                                           "key1",
                                            comment,
                                            1,
                                            3))) {
        INIOUT(printf("Failed to update comment %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    /* Delete the key */
    if ((error = ini_config_delete_value(in_cfg,
                                         "one",
                                         COL_DSP_NDUP,
                                         "key1",
                                         4))) {
        INIOUT(printf("Failed to delete the key %d.\n", error));
        print_configuration(in_cfg, stdout);
        ini_config_destroy(in_cfg);
        return error;
    }

    file = fopen(resname, "w");
    print_configuration(in_cfg, file);
    fclose(file);
    ini_config_destroy(in_cfg);

    snprintf(command, PATH_MAX * 3, "diff -wi %s %s", srcname, resname);
    error = system(command);
    INIOUT(printf("Comparison of %s %s returned: %d\n",
                  srcname, resname, error));

    if ((error) || (WEXITSTATUS(error))) {
        printf("Failed to run diff command %d %d.\n",  error,
               WEXITSTATUS(error));
        return -1;
    }


    INIOUT(printf("<==== End ====>\n"));

    return EOK;
}

int main(int argc, char *argv[])
{
    int error = EOK;
    test_fn tests[] = { basic_test,
                        dup_test,
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
