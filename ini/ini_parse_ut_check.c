/*
    INI LIBRARY

    Check based unit test for ini parser.

    Copyright (C) Michal Zidek <mzidek@redhat.com> 2016

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

#define TEST_DIR_PATH ""

START_TEST(test_ini_parse_non_kvp)
{
    int ret;
    struct ini_cfgobj *ini_cfg;
    int value;
    struct ini_cfgfile *file_ctx;
    struct value_obj *vo;
    char non_kvp_cfg[] =
        "[section_before]\n"
        "one = 1\n"

        "[section_non_kvp]\n"
        "two = 2\n"
        "non_kvp\n"
        "three = 3\n"
        "=nonkvp\n"

        "[section_after]\n"
        "four = 4\n";

    ret = ini_config_file_from_mem(non_kvp_cfg, strlen(non_kvp_cfg),
                                   &file_ctx);
    fail_unless(ret == EOK, "Failed to load config. Error %d.\n", ret);

    /* First try without the INI_PARSE_IGNORE_NON_KVP. This should fail
     * with error. */
    ret = ini_config_create(&ini_cfg);
    fail_unless(ret == EOK, "Failed to create config. Error %d.\n", ret);
    ret = ini_config_parse(file_ctx, INI_STOP_ON_ERROR, INI_MV1S_ALLOW, 0,
                           ini_cfg);
    fail_if(ret != 5, "Expected error was not found.\n");

    ini_config_destroy(ini_cfg);
    ini_config_file_destroy(file_ctx);

    /* Now try with INI_PARSE_IGNORE_NON_KVP. We should have no errors
     * and all the surounding configuration should be valid */
    ret = ini_config_file_from_mem(non_kvp_cfg, strlen(non_kvp_cfg),
                                   &file_ctx);
    fail_unless(ret == EOK, "Failed to load config. Error %d.\n", ret);
    ret = ini_config_create(&ini_cfg);
    fail_unless(ret == EOK, "Failed to create config. Error %d.\n", ret);
    ret = ini_config_parse(file_ctx, INI_STOP_ON_ERROR, INI_MV1S_ALLOW,
                           INI_PARSE_IGNORE_NON_KVP,
                           ini_cfg);
    fail_unless(ret == EOK, "ini_config_parse returned %d\n", ret);

    /* Now check if the surrounding configuration is OK */
    /* section_before */
    ret = ini_get_config_valueobj("section_before", "one", ini_cfg,
                                  INI_GET_FIRST_VALUE, &vo);
    fail_unless(ret == EOK, "ini_get_config_valueobj returned %d\n: %s", ret,
                strerror(ret));

    value = ini_get_int_config_value(vo, 1, -1, &ret);
    fail_unless(ret == EOK, "ini_get_int_config_value returned %d\n: %s", ret,
                strerror(ret));

    fail_unless(ret == EOK);
    fail_if(value != 1, "Expected value 1 got %d\n", value);

    /* section_non_kvp */
    ret = ini_get_config_valueobj("section_non_kvp", "two", ini_cfg,
                                  INI_GET_FIRST_VALUE, &vo);
    fail_unless(ret == EOK);

    value = ini_get_int_config_value(vo, 1, -1, &ret);
    fail_unless(ret == EOK);
    fail_if(value != 2, "Expected value 2 got %d\n", value);

    ret = ini_get_config_valueobj("section_non_kvp", "three", ini_cfg,
                                  INI_GET_FIRST_VALUE, &vo);
    fail_unless(ret == EOK);

    value = ini_get_int_config_value(vo, 1, -1, &ret);
    fail_unless(ret == EOK);
    fail_if(value != 3, "Expected value 3 got %d\n", value);

    /* section_after */
    ret = ini_get_config_valueobj("section_after", "four", ini_cfg,
                                  INI_GET_FIRST_VALUE, &vo);
    fail_unless(ret == EOK);

    value = ini_get_int_config_value(vo, 1, -1, &ret);
    fail_unless(ret == EOK);
    fail_if(value != 4, "Expected value 4 got %d\n", value);

    ini_config_destroy(ini_cfg);
    ini_config_file_destroy(file_ctx);
}
END_TEST

START_TEST(test_ini_parse_section_key_conflict)
{
    /*
     * This tests the behavior of ini_config_parse to ensure correct handling
     * of conflicts between sections and keys of the same name. There are
     * three possibilities for conflict:
     *
     *   1. Inside a section, between the section name and a key name
     *   2. Between a default-section key name and a section name
     *   3. Between a key name in a different section and a section name
     *
     *   In case (1), parsing finished without an error. However, when
     *   trying to select a value object inside a section, the returned
     *   object was an unchecked cast from the section's data, and not the
     *   attribute's data. In cases (2) and (3), the parser segfaulted while
     *   trying to merge a section with an attribute.
     */

    char config1[] =
        "[a]\n"
        "a=a\n";

    char config2[] =
        "a=b\n"
        "[a]\n"
        "c=d\n";

    char config3[] =
        "[a]\n"
        "b=c\n"
        "[b]\n"
        "a=d\n";

    char *file_contents[] = {config1, config2, config3, NULL};

    size_t iter;

    struct ini_cfgobj *ini_config = NULL;
    struct ini_cfgfile *file_ctx = NULL;

    int ret;
    int i;
    int j;

    char **sections = NULL;
    int sections_count = 0;
    int sections_error = 0;

    char **attributes = NULL;
    int attributes_count = 0;
    int attributes_error = 0;

    struct value_obj *val = NULL;
    char *val_str = NULL;

    for (iter = 0; file_contents[iter] != NULL; iter++) {
        ret = ini_config_create(&ini_config);
        fail_unless(ret == EOK, "Failed to create config. Error %d.\n", ret);

        ret = ini_config_file_from_mem(file_contents[iter],
                                       strlen(file_contents[iter]),
                                       &file_ctx);
        fail_unless(ret == EOK, "Failed to load file. Error %d.\n", ret);

        ret = ini_config_parse(file_ctx, 1, 0, 0, ini_config);
        fail_unless(ret == EOK, "Failed to parse file. Error %d.\n", ret);

        sections = ini_get_section_list(ini_config, &sections_count,
                                        &sections_error);
        fail_unless(sections_error == EOK,
                    "Failed to get sections. Error %d.\n",
                    sections_error);

        for (i = 0; i < sections_count; i++) {
            attributes = ini_get_attribute_list(ini_config,
                                                sections[i],
                                                &attributes_count,
                                                &attributes_error);
            fail_unless(attributes_error == EOK,
                        "Failed to get attributes. Error %d.\n",
                        attributes_error);

            for (j = 0; j < attributes_count; j++) {
                ret = ini_get_config_valueobj(sections[i], attributes[j],
                                              ini_config, 0, &val);
                fail_unless(ret == EOK,
                            "Failed to get attribute. Error %d.\n",
                            ret);

                val_str = ini_get_string_config_value(val, &ret);
                fail_unless(ret == EOK,
                            "Failed to get attribute as string. Error %d.\n",
                            ret);
                fail_unless(val_str != NULL,
                            "Failed to get attribute as string: was NULL.\n");

                free(val_str);
            }

            ini_free_attribute_list(attributes);
        }

        ini_free_section_list(sections);
        ini_config_file_destroy(file_ctx);
        ini_config_destroy(ini_config);
    }
}
END_TEST

/* Maybe we should test even bigger values? */
#define VALUE_LEN 10000
/* The +100 is space for section name and key name. */
#define CFGBUF_LEN (VALUE_LEN + 100)
START_TEST(test_ini_long_value)
{
    int ret;
    struct ini_cfgobj *ini_cfg;
    struct ini_cfgfile *file_ctx;
    struct value_obj *vo;
    char big_val_cfg[CFGBUF_LEN] = {0};
    char value[VALUE_LEN] = {0};
    char *value_got;

    /* The value is just a lot of As ending with '\0'*/
    memset(value, 'A', VALUE_LEN - 1);

    /* Create config file */
    ret = snprintf(big_val_cfg, CFGBUF_LEN, "[section]\nkey=%s", value);

    ret = ini_config_file_from_mem(big_val_cfg, strlen(big_val_cfg),
                                   &file_ctx);
    fail_unless(ret == EOK, "Failed to load config. Error %d.\n", ret);

    ret = ini_config_create(&ini_cfg);
    fail_unless(ret == EOK, "Failed to create config. Error %d.\n", ret);
    ret = ini_config_parse(file_ctx, INI_STOP_ON_ERROR, INI_MV1S_ALLOW, 0,
                           ini_cfg);
    fail_if(ret != 0, "Failed to parse config. Error %d.\n", ret);

    ret = ini_get_config_valueobj("section", "key", ini_cfg,
                                  INI_GET_FIRST_VALUE, &vo);
    fail_unless(ret == EOK, "ini_get_config_valueobj returned %d\n: %s", ret,
                strerror(ret));

    value_got = ini_get_string_config_value(vo, &ret);
    fail_unless(ret == EOK, "ini_get_int_config_value returned %d\n: %s", ret,
                strerror(ret));

    fail_unless(strcmp(value, value_got) == 0, "Expected and found values differ!\n");
    free(value_got);
    ini_config_destroy(ini_cfg);
    ini_config_file_destroy(file_ctx);
}
END_TEST

static Suite *ini_parse_suite(void)
{
    Suite *s = suite_create("ini_parse_suite");

    TCase *tc_parse = tcase_create("ini_parse");
    tcase_add_test(tc_parse, test_ini_parse_non_kvp);
    tcase_add_test(tc_parse, test_ini_parse_section_key_conflict);
    tcase_add_test(tc_parse, test_ini_long_value);

    suite_add_tcase(s, tc_parse);

    return s;
}

int main(void)
{
    int number_failed;

    Suite *s = ini_parse_suite();
    SRunner *sr = srunner_create(s);
    /* If CK_VERBOSITY is set, use that, otherwise it defaults to CK_NORMAL */
    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
