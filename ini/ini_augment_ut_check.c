/*
    INI LIBRARY

    Check based unit test for ini_config_augment.

    Copyright (C) Alexander Scheel <ascheel@redhat.com> 2017

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
#include <stdlib.h>
#include <check.h>

/* #define TRACE_LEVEL 7 */
#define TRACE_HOME
#include "trace.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"

static int write_to_file(char *path, char *text)
{
    FILE *f = fopen(path, "w");
    int bytes = 0;
    if (f == NULL)
        return 1;

    bytes = fprintf(f, "%s", text);
    if (bytes != strlen(text)) {
        return 1;
    }

    return fclose(f);
}

static int exists_array(const char *needle, char **haystack, uint32_t count)
{
    uint32_t i = 0;

    for (i = 0; i < count; i++) {
        fprintf(stderr, "%s == %s?\n", needle, haystack[i]);
        if (strcmp(needle, haystack[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

START_TEST(test_ini_augment_merge_sections)
{
    char base_path[PATH_MAX];
    char augment_path[PATH_MAX];

    char config_base[] =
        "[section]\n"
        "key1 = first\n"
        "key2 = exists\n";

    char config_augment[] =
        "[section]\n"
        "key1 = augment\n"
        "key3 = exists\n";

    char *builddir;

    uint32_t flags[3] = { INI_MS_DETECT , INI_MS_DETECT | INI_MS_PRESERVE,
                          INI_MS_DETECT | INI_MS_OVERWRITE };

    int expected_attributes_counts[3] = { 3, 2, 2 };
    const char *test_sections[3] = { "section", "section", "section" };
    const char *test_attributes[3] = { "key3", "key1", "key1" };
    const char *test_attribute_values[3] = {"exists", "first", "augment" };

    int ret;
    int iter;

    builddir = getenv("builddir");
    if (builddir == NULL) {
        builddir = strdup(".");
    }

    snprintf(base_path, PATH_MAX, "%s/tmp_augment_base.conf", builddir);
    snprintf(augment_path, PATH_MAX, "%s/tmp_augment_augment.conf", builddir);

    ret = write_to_file(base_path, config_base);
    fail_unless(ret == 0, "Failed to write %s: ret %d.\n", base_path, ret);

    write_to_file(augment_path, config_augment);
    fail_unless(ret == 0, "Failed to write %s: ret %d.\n", augment_path, ret);

    for (iter = 0; iter < 3; iter++) {
        uint32_t merge_flags = flags[iter];
        int expected_attributes_count = expected_attributes_counts[iter];
        const char *test_section = test_sections[iter];
        const char *test_attribute = test_attributes[iter];
        const char *test_attribute_value = test_attribute_values[iter];
        struct ini_cfgobj *in_cfg;
        struct ini_cfgobj *result_cfg;
        struct ini_cfgfile *file_ctx;
        struct ref_array *error_list;
        struct ref_array *success_list;

        char **sections;
        int sections_count;

        char **attributes;
        int attributes_count;

        struct value_obj *val;
        char *val_str;

        /* Match only augment.conf */
        const char *m_patterns[] = { "^tmp_augment_augment.conf$", NULL };

        /* Match all sections */
        const char *m_sections[] = { ".*", NULL };

        /* Create config collection */
        ret = ini_config_create(&in_cfg);
        fail_unless(ret == EOK, "Failed to create collection. Error %d\n",
                    ret);

        /* Open base.conf */
        ret = ini_config_file_open(base_path, 0, &file_ctx);
        fail_unless(ret == EOK, "Failed to open file. Error %d\n", ret);

        /* Seed in_cfg with base.conf */
        ret = ini_config_parse(file_ctx, 1, 0, 0, in_cfg);
        fail_unless(ret == EOK, "Failed to parse file context. Error %d\n",
                    ret);

        /* Update base.conf with augment.conf */
        ret = ini_config_augment(in_cfg,
                                 builddir,
                                 m_patterns,
                                 m_sections,
                                 NULL,
                                 INI_STOP_ON_NONE,
                                 0,
                                 INI_PARSE_NOSPACE|INI_PARSE_NOTAB,
                                 merge_flags,
                                 &result_cfg,
                                 &error_list,
                                 &success_list);
        /* We always expect EEXIST due to DETECT being set. */
        fail_unless(ret == EEXIST,
                    "Failed to augment context. Error %d\n", ret);

        if (result_cfg) {
            ini_config_destroy(in_cfg);
            in_cfg = result_cfg;
            result_cfg = NULL;
        }

        /* Get a list of sections from the resulting cfg. */
        sections = ini_get_section_list(in_cfg, &sections_count, &ret);
        fail_unless(ret == EOK, "Failed to get section list. Error %d\n", ret);

        /* Validate that the tested section exists. */
        ret = exists_array(test_section, sections, sections_count);
        fail_if(ret == 0, "Failed to find expected section.\n");

        /* Get a list of attributes from the resulting cfg. */
        attributes = ini_get_attribute_list(in_cfg, test_section,
                                            &attributes_count,
                                            &ret);
        fail_unless(ret == EOK, "Failed to get attribute list. Error %d\n",
                    ret);

        /* Validate that the expected number of attributes exist. This
         * distinguishes MERGE from PRESERVE/OVERWRITE. */
        fail_unless(expected_attributes_count == attributes_count,
                    "Expected %d attributes, but received %d.\n",
                    expected_attributes_count, attributes_count);

        /* Validate that the test attribute exists. This distinguishes
         * PRESERVE from OVERWRITE. */
        ret = exists_array(test_attribute, attributes, attributes_count);
        fail_if(ret == 0, "Failed to find expected attribute.\n");

        ret = ini_get_config_valueobj(test_section, test_attribute, in_cfg,
                                        0, &val);
        fail_unless(ret == EOK, "Failed to load value object. Error %d\n",
                    ret);

        val_str = ini_get_string_config_value(val, &ret);
        fail_unless(ret == EOK, "Failed to get config value. Error %d\n", ret);

        /* Validate the value of the test attribute. */
        ret = strcmp(val_str, test_attribute_value);

        fail_unless(ret == 0, "Attribute %s didn't have expected value of "
                    "(%s): saw %s\n", test_attribute, test_attribute_value,
                    val_str);

        /* Cleanup */
        free(val_str);
        ini_free_attribute_list(attributes);
        ini_free_section_list(sections);
        ref_array_destroy(error_list);
        ini_config_file_destroy(file_ctx);
        ref_array_destroy(success_list);
        ini_config_destroy(in_cfg);
        ini_config_destroy(result_cfg);
    }

    remove(base_path);
    remove(augment_path);
    free(builddir);
}
END_TEST

START_TEST(test_ini_augment_empty_dir)
{
    int ret;
    struct ini_cfgobj *ini_cfg;
    struct ini_cfgfile *file_ctx;
    struct value_obj *vo;
    const char *patterns[] = { ".*", NULL };
    const char *sections[] = { ".*", NULL };
    char **section_list;
    char **attrs_list;
    struct ini_cfgobj *result_cfg = NULL;
    int size;
    char empty_dir_path[PATH_MAX] = {0};
    char *builddir;
    int32_t val;
    char base_cfg[] =
        "[section_one]\n"
        "one = 1\n";

    builddir = getenv("builddir");
    if (builddir == NULL) {
        builddir = strdup(".");
    }

    ret = snprintf(empty_dir_path, PATH_MAX, "%s/tmp_empty_dir", builddir);
    fail_if(ret > PATH_MAX || ret < 0, "snprintf failed\n");

    ret = ini_config_file_from_mem(base_cfg, strlen(base_cfg),
                                   &file_ctx);
    fail_unless(ret == EOK, "Failed to load config. Error %d.\n", ret);

    ret = ini_config_create(&ini_cfg);
    fail_unless(ret == EOK, "Failed to create config. Error %d.\n", ret);
    ret = ini_config_parse(file_ctx, INI_STOP_ON_ERROR, INI_MV1S_ALLOW, 0,
                           ini_cfg);
    fail_unless(ret == EOK, "Failed to parse configuration. Error %d.\n", ret);

    /* Create an empty directory */
    ret = mkdir(empty_dir_path, 0700);
    if (ret == -1) {
        ret = errno;
        fail_if(ret != EEXIST,
                "Failed to create empty directory. Error %d.\n", errno);
    }

    ret = ini_config_augment(ini_cfg,
                             empty_dir_path,
                             patterns,
                             sections,
                             NULL,
                             INI_STOP_ON_ANY,
                             INI_MV1S_OVERWRITE,
                             INI_PARSE_NOWRAP,
                             INI_MV2S_OVERWRITE,
                             &result_cfg,
                             NULL,
                             NULL);

    fail_unless(ret == EOK);

    /* If the snippet directory is empty, result_cfg should be the original
     * ini_cfg and not NULL */
    fail_if(result_cfg == NULL);

    /* Now check if the content of result_cfg is what we expected */
    section_list = ini_get_section_list(result_cfg, &size, NULL);
    fail_unless(size == 1);
    fail_unless(strcmp(section_list[0], "section_one") == 0);

    attrs_list = ini_get_attribute_list(result_cfg, section_list[0],
                                        &size, NULL);
    fail_unless(size == 1);
    fail_unless(strcmp(attrs_list[0], "one") == 0);

    ret = ini_get_config_valueobj(section_list[0],
                                  attrs_list[0],
                                  result_cfg,
                                  INI_GET_FIRST_VALUE,
                                  &vo);
    fail_unless(ret == 0);

    val = ini_get_int32_config_value(vo, 1, 100, NULL);
    fail_unless(val == 1, "Expected attribute value not found.\n");

    ini_config_destroy(ini_cfg);
    ini_config_file_destroy(file_ctx);
    remove(empty_dir_path);
}
END_TEST

static Suite *ini_augment_suite(void)
{
    Suite *s = suite_create("ini_augment_suite");

    TCase *tc_augment = tcase_create("ini_augment");
    tcase_add_test(tc_augment, test_ini_augment_merge_sections);
    tcase_add_test(tc_augment, test_ini_augment_empty_dir);

    suite_add_tcase(s, tc_augment);

    return s;
}

int main(void)
{
    int number_failed;

    Suite *s = ini_augment_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
