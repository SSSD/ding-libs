/*
    INI LIBRARY

    Unit test for the configuration file validators API.

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
#include <string.h>
#include <stdlib.h>
#include <check.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* #define TRACE_LEVEL 7 */
#define TRACE_HOME
#include "trace.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"

#define TEST_DIR_PATH ""
#define TEST_RULES_FILE TEST_DIR_PATH"test_rules.ini"

static void create_rules_from_str(const char *rules,
                                  struct ini_cfgobj **_rules_obj)
{
    FILE *file;
    size_t written;
    int ret;

    /* We want to test actual reading from file using
     * ini_rules_read_from_file, so we create the file here */
    file = fopen(TEST_RULES_FILE, "w");
    fail_if(file == NULL, "fopen() failed: %s", strerror(errno));
    written = fwrite(rules, 1, strlen(rules), file);
    fail_unless(written == strlen(rules));

    /* allow reading */
    ret = chmod(TEST_RULES_FILE, 0664);
    fail_unless(ret == 0, "chmod() failed: %s", strerror(errno));

    fclose(file);

    ret = ini_rules_read_from_file(TEST_RULES_FILE, _rules_obj);
    fail_unless(ret == 0, "read_rules_from_file() failed: %s", strerror(ret));
}

static struct ini_cfgobj *get_ini_config_from_str(char input_data[],
                                                  size_t input_data_len)
{
    struct ini_cfgobj *in_cfg;
    struct ini_cfgfile *file_ctx;
    int ret;

    ret = ini_config_create(&in_cfg);
    fail_unless(ret == EOK, "Failed to create config. Error %d.\n", ret);

    ret = ini_config_file_from_mem(input_data, input_data_len, &file_ctx);
    fail_unless(ret == EOK, "Failed to load config. Error %d.\n", ret);

    ret = ini_config_parse(file_ctx, INI_STOP_ON_NONE, INI_MV1S_ALLOW, 0,
                           in_cfg);
    fail_unless(ret == EOK, "Failed to parse config. Error %d.\n", ret);

    ini_config_file_destroy(file_ctx);

    return in_cfg;
}

START_TEST(test_ini_errobj)
{
    struct ini_errobj *errobj;
    int ret;
    const char TEST_MSG1[] = "Test message one.";
    const char TEST_MSG2[] = "Test message two.";
    const char TEST_MSG3[] = "Test message three.";

    ret = ini_errobj_create(NULL);
    fail_unless(ret == EINVAL,
                "ini_errobj_create(NULL) failed with wrong error [%s]",
                strerror(ret));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    /* We just created the errobj, it should be empty */
    fail_unless(ini_errobj_no_more_msgs(errobj));

    /* Now add three messages, after adding each message,
     * check if the errobj has correct content. */
    ret = ini_errobj_add_msg(errobj, TEST_MSG1);
    fail_if(ret != 0, "ini_errobj_add_msg() failed: %s", strerror(ret));
    fail_if(ini_errobj_no_more_msgs(errobj));
    ret = strcmp(TEST_MSG1, ini_errobj_get_msg(errobj));
    fail_if(ret != 0, "TEST_MSG1 was not found.");
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ret = ini_errobj_add_msg(errobj, TEST_MSG2);
    fail_if(ret != 0, "ini_errobj_add_msg() failed: %s", strerror(ret));
    ini_errobj_reset(errobj); /* strart from first message */
    fail_if(ini_errobj_no_more_msgs(errobj));
    ret = strcmp(TEST_MSG1, ini_errobj_get_msg(errobj));
    fail_if(ret != 0, "TEST_MSG1 was not found.");
    ini_errobj_next(errobj);
    fail_if(ini_errobj_no_more_msgs(errobj));
    ret = strcmp(TEST_MSG2, ini_errobj_get_msg(errobj));
    fail_if(ret != 0, "TEST_MSG2 was not found.");
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ret = ini_errobj_add_msg(errobj, TEST_MSG3);
    fail_if(ret != 0, "ini_errobj_add_msg() failed: %s", strerror(ret));
    ini_errobj_reset(errobj); /* strart from first message */
    fail_if(ini_errobj_no_more_msgs(errobj));
    ret = strcmp(TEST_MSG1, ini_errobj_get_msg(errobj));
    fail_if(ret != 0, "TEST_MSG1 was not found.");
    ini_errobj_next(errobj);
    fail_if(ini_errobj_no_more_msgs(errobj));
    ret = strcmp(TEST_MSG2, ini_errobj_get_msg(errobj));
    fail_if(ret != 0, "TEST_MSG2 was not found.");
    ini_errobj_next(errobj);
    fail_if(ini_errobj_no_more_msgs(errobj));
    ret = strcmp(TEST_MSG3, ini_errobj_get_msg(errobj));
    fail_if(ret != 0, "TEST_MSG3 was not found.");
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
}
END_TEST

START_TEST(test_ini_noerror)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;

    char input_rules[] =
        "[rule/always_succeed]\n"
        "validator = ini_dummy_noerror\n";

    char input_cfg[] =
        "[section]\n"
        "# Content of this file should not matter\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_error)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;
    const char *errmsg;

    char input_rules[] =
        "[rule/generate_error]\n"
        "validator = ini_dummy_error\n";

    char input_wrong_rule[] =
        "[rule/generate_error]\n"
        "valid = ini_dummy_error\n";

    char input_cfg[] =
        "[section]\n"
        "# Content of this file should not matter\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate exactly one error */
    fail_if(ini_errobj_no_more_msgs(errobj));
    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg, "[rule/generate_error]: Error");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_rules_destroy(rules_obj);

    /* test rule with missing validator */
    create_rules_from_str(input_wrong_rule, &rules_obj);

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate exactly one error */
    fail_if(ini_errobj_no_more_msgs(errobj));
    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg, "Rule 'rule/generate_error' has no validator.");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_rules_destroy(rules_obj);
    ini_config_destroy(cfg_obj);
}
END_TEST

START_TEST(test_unknown_validator)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;

    char input_rules[] =
        "[rule/always_succeed]\n"
        "validator = nonexistent_validator\n";

    char input_cfg[] =
        "[section]\n"
        "# Content of this file should not matter\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate exactly one error */
    fail_if(ini_errobj_no_more_msgs(errobj));
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

static int custom_noerror(const char *rule_name,
                          struct ini_cfgobj *rules_obj,
                          struct ini_cfgobj *config_obj,
                          struct ini_errobj *errobj,
                          void **data)
{
    return 0;
}

static int custom_error(const char *rule_name,
                        struct ini_cfgobj *rules_obj,
                        struct ini_cfgobj *config_obj,
                        struct ini_errobj *errobj,
                        void **data)
{
    return ini_errobj_add_msg(errobj, "Error");
}

START_TEST(test_custom_noerror)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;
    struct ini_validator *noerror[] = {
        &(struct ini_validator){ "custom_noerror", custom_noerror, NULL },
        NULL
    };
    struct ini_validator *missing_name[] = {
        &(struct ini_validator){ NULL, custom_noerror, NULL },
        &(struct ini_validator){ "custom_noerror", custom_noerror, NULL },
        NULL
    };

    char input_rules[] =
        "[rule/custom_succeed]\n"
        "validator = custom_noerror\n";

    char input_cfg[] =
        "[section]\n"
        "# Content of this file should not matter\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    /* Pass the custom validator to ini_rules_check() */
    ret = ini_rules_check(rules_obj, cfg_obj, noerror, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate no errors */
    fail_unless(ini_errobj_no_more_msgs(errobj));

    /* Pass wrong external validator to ini_rules_check() */
    /* It should be skipped */
    ret = ini_rules_check(rules_obj, cfg_obj, missing_name, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate no errors */
    fail_unless(ini_errobj_no_more_msgs(errobj), "%s", ini_errobj_get_msg(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_custom_error)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;
    struct ini_validator *error[] = {
        &(struct ini_validator){ "custom_error", custom_error, NULL },
        NULL
    };
    struct ini_validator *missing_function[] = {
        &(struct ini_validator){ "custom_noerror", NULL, NULL },
        NULL
    };
    const char *errmsg;

    char input_rules[] =
        "[rule/custom_error]\n"
        "validator = custom_error\n";

    char input_cfg[] =
        "[section]\n"
        "# Content of this file should not matter\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    /* Pass the custom validator to ini_rules_check() */
    ret = ini_rules_check(rules_obj, cfg_obj, error, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate one error */
    fail_if(ini_errobj_no_more_msgs(errobj));
    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg, "[rule/custom_error]: Error");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    /* Pass the custom validator to ini_rules_check() */
    ret = ini_rules_check(rules_obj, cfg_obj, missing_function, errobj);

    /* Should generate one error for missing validator */
    fail_if(ini_errobj_no_more_msgs(errobj));
    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "Rule 'rule/custom_error' uses unknown validator "
                 "'custom_error'.");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);

    ini_rules_destroy(rules_obj);
    ini_config_destroy(cfg_obj);
}
END_TEST

START_TEST(test_ini_allowed_options_ok)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;

    /* Only bar and baz are allowed for foo section */
    char input_rules[] =
        "[rule/options_for_foo]\n"
        "validator = ini_allowed_options\n"
        "section_re = ^foo$\n"
        "option = bar\n"
        "option = baz\n";

    /* Should check only foo section, other sections are
     * irrelevant and can contain any option */
    char input_cfg[] =
        "[foo]\n"
        "bar = 0\n"
        "baz = 0\n"
        "[oof]\n"
        "opt1 = 1\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate no errors */
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_options_no_section)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;
    size_t num_err;
    const char *errmsg;

    /* Ommit section_re to generate error */
    char input_rules[] =
        "[rule/options_for_foo]\n"
        "validator = ini_allowed_options\n"
        /* "section_re = ^foo$\n" */
        "option = bar\n"
        "option = baz\n";

    /* section_re without value */
    char input_rules2[] =
        "[rule/options_for_foo]\n"
        "validator = ini_allowed_options\n"
        "section_re = \n"
        "option = bar\n"
        "option = baz\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[foo]\n"
        "bar = 0\n"
        "baz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 2 errors (one from rules_check and one
     * from the validator itself) */
    fail_if(ini_errobj_no_more_msgs(errobj));

    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 2, "Expected 2 errors, got %d", num_err);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "Rule 'rule/options_for_foo' returned error code '22'");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "[rule/options_for_foo]: Validator misses 'section_re' "
                 "parameter");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_rules_destroy(rules_obj);

    /* the second test with missing value for section_re */

    create_rules_from_str(input_rules2, &rules_obj);

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 2 errors (one from rules_check and one
     * from the validator itself) */
    fail_if(ini_errobj_no_more_msgs(errobj));

    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 2, "Expected 2 errors, got %d", num_err);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "Rule 'rule/options_for_foo' returned error code '22'");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "[rule/options_for_foo]: Validator misses 'section_re' "
                 "parameter");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);
    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_rules_destroy(rules_obj);

    ini_config_destroy(cfg_obj);
}
END_TEST

START_TEST(test_ini_allowed_options_wrong_regex)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;
    size_t num_err;
    const char *errmsg;

    /* Ommit section_re to generate error */
    char input_rules[] =
        "[rule/options_for_foo]\n"
        "validator = ini_allowed_options\n"
        "section_re = ^foo[$\n"
        "option = bar\n"
        "option = baz\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[foo]\n"
        "bar = 0\n"
        "baz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 2 errors (one from rules_check and one
     * from the validator itself) */
    fail_if(ini_errobj_no_more_msgs(errobj));

    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 2, "Expected 2 errors, got %d", num_err);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "Rule 'rule/options_for_foo' returned error code '22'");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "[rule/options_for_foo]: Cannot compile regular expression "
                 "from option 'section_re'. "
                 "Error: 'Unmatched [ or [^'");
    if (ret != 0) {
        ret = strcmp(errmsg,
                     "[rule/options_for_foo]: Cannot compile regular expression "
                     "from option 'section_re'. "
                     "Error: 'brackets ([ ]) not balanced'");
    }
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);

    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_options_typos)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;
    size_t num_err;

    /* Only bar and baz are allowed for foo section */
    char input_rules[] =
        "[rule/options_for_foo]\n"
        "validator = ini_allowed_options\n"
        "section_re = ^foo$\n"
        "option = bar\n"
        "option = baz\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[foo]\n"
        "br = 0\n"
        "bra = 0\n"
        "abr = 0\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 4 errors */
    fail_if(ini_errobj_no_more_msgs(errobj));

    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 4, "Expected 4 errors, got %d", num_err);

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_str_ok)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;

    /* Only bar and baz are allowed for foo section */
    char input_rules[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n"
        "section = foo\n"
        "section = bar\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[foo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[bar]\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 0 errors */
    fail_unless(ini_errobj_no_more_msgs(errobj),
                "Unexpected errors found: [%s]", ini_errobj_get_msg(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_str_typos)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int num_err;
    int ret;

    /* Only bar and baz are allowed for foo section */
    char input_rules[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n"
        "section = foo\n"
        "section = bar\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[fooo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[baar]\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 2 errors */
    fail_if(ini_errobj_no_more_msgs(errobj),
            "Expected 2 errors but none found");
    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 2, "Expected 2 errors, got %d", num_err);

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_str_insensitive)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;
    int i;

    /* Only bar and baz are allowed for foo section */
    char input_rules_template[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n"
        "case_insensitive = %s\n"
        "section = foo\n"
        "section = bar\n";

    char input_rules[sizeof(input_rules_template) + 10];

    const char *case_insensitive_values[] = { "yes", "Yes", "true", "True",
                                              "1", NULL };
    /* Make 4 typos */
    char input_cfg[] =
        "[FOo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[baR]\n"
        "abz = 0\n";

    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    for (i = 0; case_insensitive_values[i] != NULL; i++) {
        sprintf(input_rules, input_rules_template, case_insensitive_values[i]);

        create_rules_from_str(input_rules, &rules_obj);

        ret = ini_errobj_create(&errobj);
        fail_unless(ret == 0,
                    "ini_errobj_create() failed for case_insensitive = %s: %s",
                    case_insensitive_values[i], strerror(ret));

        ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
        fail_unless(ret == 0,
                    "ini_rules_check() failed for case_insensitive = %s: %s",
                    case_insensitive_values[i], strerror(ret));

        /* Should generate 0 errors */
        fail_unless(ini_errobj_no_more_msgs(errobj),
                    "Unexpected errors found for case_insensitive = %s: [%s]",
                    case_insensitive_values[i], ini_errobj_get_msg(errobj));

        ini_errobj_destroy(&errobj);
        ini_rules_destroy(rules_obj);
    }

    ini_config_destroy(cfg_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_re_ok)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;

    char input_rules[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n"
        "section_re = ^foo*$\n"
        "section_re = bar\n";

    char input_cfg[] =
        "[foooooooooooo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[my_bar]\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 0 errors */
    fail_unless(ini_errobj_no_more_msgs(errobj), "Unexpected errors found");

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_re_typos)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int num_err;
    int ret;

    char input_rules[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n"
        "section_re = ^foo*$\n"
        "section_re = bar\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[fooooooOooooo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[my_bra]\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 2 errors */
    fail_if(ini_errobj_no_more_msgs(errobj),
            "Expected 2 errors but none found");
    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 2, "Expected 2 errors, got %d", num_err);

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_re_insensitive)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int ret;

    /* Only bar and baz are allowed for foo section */
    char input_rules[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n"
        "case_insensitive = yes\n"
        "section_re = ^foo*$\n"
        "section_re = bar\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[FOoOoOoOoOOOOooo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[my_Bar]\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 0 errors */
    fail_unless(ini_errobj_no_more_msgs(errobj), "Unexpected errors found");

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_missing_section)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int num_err;
    int ret;
    const char *errmsg;

    /* Only bar and baz are allowed for foo section */
    char input_rules[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[fooo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[baar]\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 1 errors */
    fail_if(ini_errobj_no_more_msgs(errobj),
            "Expected 1 errors but none found");
    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 1, "Expected 1 errors, got %d", num_err);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "[rule/section_list]: No allowed sections specified. "
                 "Use 'section = default' to allow only default section");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);

    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

START_TEST(test_ini_allowed_sections_wrong_regex)
{
    struct ini_cfgobj *rules_obj;
    struct ini_cfgobj *cfg_obj;
    struct ini_errobj *errobj;
    int num_err;
    int ret;
    const char *errmsg;

    /* Only bar and baz are allowed for foo section */
    char input_rules[] =
        "[rule/section_list]\n"
        "validator = ini_allowed_sections\n"
        "section_re = ^foo\\(*$\n";

    /* Make 4 typos */
    char input_cfg[] =
        "[fooo]\n"
        "br = 0\n"
        "bra = 0\n"
        "[baar]\n"
        "abz = 0\n";

    create_rules_from_str(input_rules, &rules_obj);
    cfg_obj = get_ini_config_from_str(input_cfg, sizeof(input_cfg));

    ret = ini_errobj_create(&errobj);
    fail_unless(ret == 0, "ini_errobj_create() failed: %s", strerror(ret));

    ret = ini_rules_check(rules_obj, cfg_obj, NULL, errobj);
    fail_unless(ret == 0, "ini_rules_check() failed: %s", strerror(ret));

    /* Should generate 2 errors */
    fail_if(ini_errobj_no_more_msgs(errobj),
            "Expected 2 errors but none found");
    num_err = ini_errobj_count(errobj);
    fail_unless(num_err == 2, "Expected 2 errors, got %d", num_err);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "Rule 'rule/section_list' returned error code '22'");
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);

    errmsg = ini_errobj_get_msg(errobj);
    ret = strcmp(errmsg,
                 "[rule/section_list]: Validator failed to use regex "
                 "[^foo\\(*$]:[Unmatched ( or \\(]");
    if (ret !=0) {
        ret = strcmp(errmsg,
                     "[rule/section_list]: Validator failed to use regex "
                     "[^foo\\(*$]:[parentheses not balanced]");
    }
    fail_unless(ret == 0, "Got msg: [%s]", errmsg);
    ini_errobj_next(errobj);

    fail_unless(ini_errobj_no_more_msgs(errobj));

    ini_errobj_destroy(&errobj);
    ini_config_destroy(cfg_obj);
    ini_rules_destroy(rules_obj);
}
END_TEST

static Suite *ini_validators_utils_suite(void)
{
    Suite *s = suite_create("ini_validators");

    TCase *tc_infrastructure = tcase_create("infrastructure");
    tcase_add_test(tc_infrastructure, test_ini_errobj);
    tcase_add_test(tc_infrastructure, test_ini_noerror);
    tcase_add_test(tc_infrastructure, test_ini_error);
    tcase_add_test(tc_infrastructure, test_unknown_validator);
    tcase_add_test(tc_infrastructure, test_custom_noerror);
    tcase_add_test(tc_infrastructure, test_custom_error);

    TCase *tc_allowed_options = tcase_create("ini_allowed_options");
    tcase_add_test(tc_allowed_options, test_ini_allowed_options_ok);
    tcase_add_test(tc_allowed_options, test_ini_allowed_options_no_section);
    tcase_add_test(tc_allowed_options, test_ini_allowed_options_wrong_regex);
    tcase_add_test(tc_allowed_options, test_ini_allowed_options_typos);

    TCase *tc_allowed_sections = tcase_create("ini_allowed_sections");
    tcase_add_test(tc_allowed_sections, test_ini_allowed_sections_str_ok);
    tcase_add_test(tc_allowed_sections, test_ini_allowed_sections_str_typos);
    tcase_add_test(tc_allowed_sections,
                   test_ini_allowed_sections_str_insensitive);
    tcase_add_test(tc_allowed_sections, test_ini_allowed_sections_re_ok);
    tcase_add_test(tc_allowed_sections, test_ini_allowed_sections_re_typos);
    tcase_add_test(tc_allowed_sections,
                   test_ini_allowed_sections_re_insensitive);
    tcase_add_test(tc_allowed_sections,
                   test_ini_allowed_sections_missing_section);
    tcase_add_test(tc_allowed_sections, test_ini_allowed_sections_wrong_regex);

    suite_add_tcase(s, tc_infrastructure);
    suite_add_tcase(s, tc_allowed_options);
    suite_add_tcase(s, tc_allowed_sections);

    return s;
}

int main(void)
{
    int number_failed;

    Suite *s = ini_validators_utils_suite();
    SRunner *sr = srunner_create(s);
    /* If CK_VERBOSITY is set, use that, otherwise it defaults to CK_NORMAL */
    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
