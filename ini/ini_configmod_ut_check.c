/*
    INI LIBRARY

    Unit test for the configuration object modification API.

    Copyright (C) Lukas Slebodnik <lslebodn@redhat.com> 2015

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

/* #define TRACE_LEVEL 7 */
#define TRACE_HOME
#include "trace.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "collection_tools.h"
#include "ini_configmod.h"
#include "path_utils.h"
#include "../basicobjects/simplebuffer.h"

int verbose = 0;

#define WRAP_SIZE 80

static void dump_configuration(struct ini_cfgobj *in_cfg,
                               FILE *file)
{
    int ret;
    struct simplebuffer *sbobj = NULL;
    void *buff;
    uint32_t len;

    ret = simplebuffer_alloc(&sbobj);
    fail_unless(ret == EOK,
                "Failed to allocate buffer. Error %d.\n", ret);

    ret = ini_config_serialize(in_cfg, sbobj);
    fail_unless(ret == EOK,
                "Failed to serialize. Error %d.\n", ret);

    buff = simplebuffer_get_vbuf(sbobj);
    len = simplebuffer_get_len(sbobj);
    ret = fwrite(buff, 1, len, file);
    fail_if(ret == -1,
            "Failed to write to file. Error: %d %s\n", ret, strerror(ret));

    simplebuffer_free(sbobj);
    return;
}

static int call_diff(const char *function,
                     const char *expected_cfg,
                     size_t expected_cfg_len,
                     const char *res_cfg,
                     size_t res_cfg_len)
{
    char expected_fn[PATH_MAX];
    char res_fn[PATH_MAX];
    char command[PATH_MAX * 3];
    char *builddir;
    int ret;
    int expected_fd;
    int res_fd;

    builddir = getenv("builddir");

    snprintf(expected_fn, PATH_MAX, "%s/expected.conf_%s_XXXXXX",
             (builddir == NULL) ? "." : builddir, function);
    snprintf(res_fn, PATH_MAX, "%s/result.conf_%s_XXXXXX",
             (builddir == NULL) ? "." : builddir, function);

    expected_fd = mkstemp(expected_fn);
    fail_if(expected_fd == -1, "mkstemp failed: %s\n", strerror(errno));

    ret = write(expected_fd, expected_cfg, expected_cfg_len);
    fail_if(ret == -1,
            "Failed write to %s. Error %s\n",
            expected_fn, strerror(errno));

    close(expected_fd);

    res_fd = mkstemp(res_fn);
    fail_if(res_fd == -1, "mkstemp failed: %s\n", strerror(errno));

    ret = write(res_fd, res_cfg, res_cfg_len);
    fail_if(ret == -1,
            "Failed write to %s. Error %s\n",
            expected_fn, strerror(errno));
    close(res_fd);

    snprintf(command, PATH_MAX * 3, "diff -wi %s %s", expected_fn, res_fn);
    ret = system(command);
    fail_if(ret == -1,
            "Failed to execute command:%s. Erorr %s\n",
            command, strerror(errno));

    return EOK;
}

#define assert_configuration_equal(expected_cfg, expected_cfg_len, res_cfg) \
    _assert_configuration_equal(expected_cfg, expected_cfg_len, res_cfg, \
                                __func__, __FILE__, __LINE__)
static void _assert_configuration_equal(const char *expected_cfg,
                                        size_t expected_cfg_len,
                                        struct ini_cfgobj *res_cfg,
                                        const char *function,
                                        const char *file,
                                        int line)
{
    char *res_buffer = NULL;
    size_t res_buffer_size;
    FILE *f_memstream;
    int ret;

    --expected_cfg_len; /* do not use trailing zero */

    f_memstream = open_memstream(&res_buffer, &res_buffer_size);
    fail_if(f_memstream == NULL,
            "\n\t[%s:%d] open_memstream failed.", file, line);

    dump_configuration(res_cfg, f_memstream);
    fclose(f_memstream);

    fail_unless(expected_cfg_len == res_buffer_size,
                "\n\t[%s:%d] Size of expected config %zu and result config %d "
                "does not match. Res:%d\n",
                file, line, expected_cfg_len, res_buffer_size,
                call_diff(function, expected_cfg, expected_cfg_len,
                          res_buffer, res_buffer_size));

    ret = memcmp(res_buffer, expected_cfg, expected_cfg_len);
    fail_unless(ret == EOK,
                "\n\t[%s:%d] Configurations are not identical. Res:%d\n",
                file, line,
                call_diff(function, expected_cfg, expected_cfg_len,
                          res_buffer, res_buffer_size));

    free(res_buffer);
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

START_TEST(test_delete_value_wrong_arguments)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg = NULL;

    char exp_data[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n";

    in_cfg = get_ini_config_from_str(exp_data, sizeof(exp_data));
    assert_configuration_equal(exp_data, sizeof(exp_data), in_cfg);

    /* missing ini_config */
    ret = ini_config_delete_value(NULL, "one", COL_DSP_NDUP, "key1", 0);
    fail_unless(ret == EINVAL, "delete value should fail. Error: %d", ret);
    assert_configuration_equal(exp_data, sizeof(exp_data), in_cfg);

    /* missing section */
    ret = ini_config_delete_value(in_cfg, NULL, COL_DSP_NDUP, "key1", 0);
    fail_unless(ret == EINVAL, "delete value should fail. Error: %d", ret);
    assert_configuration_equal(exp_data, sizeof(exp_data), in_cfg);

    /* missing key */
    ret = ini_config_delete_value(in_cfg, "one", COL_DSP_NDUP, NULL, 0);
    fail_unless(ret == EINVAL, "delete value should fail. Error: %d", ret);
    assert_configuration_equal(exp_data, sizeof(exp_data), in_cfg);

    /* value index is too low */
    ret = ini_config_delete_value(in_cfg, "one", COL_DSP_NDUP, "key1", -1);
    fail_unless(ret == EINVAL, "delete value should fail. Error: %d", ret);
    assert_configuration_equal(exp_data, sizeof(exp_data), in_cfg);

    /* value index is too high */
    ret = ini_config_delete_value(in_cfg, "one", COL_DSP_NDUP, "key1", 1);
    fail_unless(ret == ENOENT, "delete value should fail. Error: %d", ret);
    assert_configuration_equal(exp_data, sizeof(exp_data), in_cfg);

    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_delete_value)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key1 = first\n"
        "key1 = second\n"
        "key1 = third\n"
        "key1 = last\n";

    char delete_first[] =
        "[zero]\n"
        "[one]\n"
        "key1 = first\n"
        "key1 = second\n"
        "key1 = third\n";

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    ret = ini_config_delete_value(in_cfg, "one", COL_DSP_NDUP, "key1", 3);
    fail_unless(ret == EOK, "delete value should fail. Error: %d", ret);
    assert_configuration_equal(delete_first, sizeof(delete_first), in_cfg);

    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_update_comments_wrong_arguments)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[one]\n"
        "key1 = value1\n"
        "key1 = value1a\n"
        "key1 = value1a_bis\n"
        "// This is a test\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key2 = value2\n"
        "key3 = value3\n";

    const char *comment[] = { "// This is a test", NULL };

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* missing ini_config */
    ret = ini_config_update_comment(NULL, "one", "key1", comment, 1, 3);
    fail_unless(ret == EINVAL, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* missing section */
    ret = ini_config_update_comment(in_cfg, NULL, "key1", comment, 1, 3);
    fail_unless(ret == EINVAL, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* missing key */
    ret = ini_config_update_comment(in_cfg, "one", NULL, comment, 1, 3);
    fail_unless(ret == EINVAL, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* wrong section */
    ret = ini_config_update_comment(in_cfg, "noexist", "key1", comment, 1, 3);
    fail_unless(ret == ENOENT, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* wrong key */
    ret = ini_config_update_comment(in_cfg, "one", "noexist", comment, 1, 3);
    fail_unless(ret == ENOENT, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* value index is too low */
    ret = ini_config_update_comment(in_cfg, "one", "key1", comment, 1, -1);
    fail_unless(ret == EINVAL, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* value index is too high */
    ret = ini_config_update_comment(in_cfg, "one", "key1", comment, 1, 5);
    fail_unless(ret == ENOENT, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_update_comments)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[one]\n"
        "key1 = value1\n"
        "key1 = value1a\n"
        "key1 = value1a_bis\n"
        "key1 = value1b\n"
        "// this is a comment\n"
        "key1 = value1c\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char exp_data_1comment[] =
        "[one]\n"
        "// This is a test1\n"
        "key1 = value1\n"
        "key1 = value1a\n"
        "key1 = value1a_bis\n"
        "key1 = value1b\n"
        "// this is a comment\n"
        "key1 = value1c\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char exp_data_2comments[] =
        "[one]\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "key1 = value1\n"
        "key1 = value1a\n"
        "key1 = value1a_bis\n"
        "key1 = value1b\n"
        "// this is a comment\n"
        "key1 = value1c\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char exp_data_1comment_after2[] =
        "[one]\n"
        "key1 = value1\n"
        "key1 = value1a\n"
        "// This is a test1\n"
        "key1 = value1a_bis\n"
        "key1 = value1b\n"
        "// this is a comment\n"
        "key1 = value1c\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char exp_replaced[] =
        "[one]\n"
        "key1 = value1\n"
        "key1 = value1a\n"
        "key1 = value1a_bis\n"
        "key1 = value1b\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "key1 = value1c\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char exp_removed_comment[] =
        "[one]\n"
        "key1 = value1\n"
        "key1 = value1a\n"
        "key1 = value1a_bis\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key2 = value2\n"
        "key3 = value3\n";

    const char *comments[] = { "// This is a test1", "// This is a test2",
                               NULL };

    const char *empty_comment[] = { NULL };

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add comments with size */
    ret = ini_config_update_comment(in_cfg, "one", "key1", comments, 1, 0);
    fail_unless(ret == EOK, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(exp_data_1comment, sizeof(exp_data_1comment),
                               in_cfg);
    ini_config_destroy(in_cfg);


    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add comments with size */
    ret = ini_config_update_comment(in_cfg, "one", "key1", comments, 2, 0);
    fail_unless(ret == EOK, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(exp_data_2comments, sizeof(exp_data_2comments),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add comments (NULL terminated array), size is 0 */
    ret = ini_config_update_comment(in_cfg, "one", "key1", comments, 0, 0);
    fail_unless(ret == EOK, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(exp_data_2comments, sizeof(exp_data_2comments),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add comments (NULL terminated array), size is 0 */
    ret = ini_config_update_comment(in_cfg, "one", "key1", comments, 1, 2);
    fail_unless(ret == EOK, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(exp_data_1comment_after2,
                               sizeof(exp_data_1comment_after2),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* replace comment */
    ret = ini_config_update_comment(in_cfg, "one", "key1", comments, 0, 4);
    fail_unless(ret == EOK, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(exp_replaced, sizeof(exp_replaced),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* remove comment */
    ret = ini_config_update_comment(in_cfg, "one", "key1",
                                    empty_comment, 0, 4);
    fail_unless(ret == EOK, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(exp_removed_comment,
                               sizeof(exp_removed_comment),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* remove comment (2nd way; argument is NULL) */
    ret = ini_config_update_comment(in_cfg, "one", "key1",
                                    NULL, 0, 4);
    fail_unless(ret == EOK, "update commants should fail. Error: %d", ret);
    assert_configuration_equal(exp_removed_comment,
                               sizeof(exp_removed_comment),
                               in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_add_str_wrong_arguments)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n";

    const char *comments[] = { "// This is a test1", "// This is a test2",
                               NULL };

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* missing ini_config */
    ret = ini_config_add_str_value(NULL, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_END, "key1",
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EINVAL, "Add str should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* missing section */
    ret = ini_config_add_str_value(in_cfg, NULL, "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_END, "key1",
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EINVAL, "Add str should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* missing key */
    ret = ini_config_add_str_value(in_cfg, "one", NULL, "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_END, "key1",
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EINVAL, "Add str should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* missing value */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", NULL,
                                   comments, 2, WRAP_SIZE, COL_DSP_END, "key1",
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EINVAL, "Add str should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* wrong index */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_END, "key1",
                                   -1, INI_VA_NOCHECK);
    fail_unless(ret == EINVAL, "Add str should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* wrong flag */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_END, "key1",
                                   0, 0xff);
    fail_unless(ret == ENOSYS, "Add str should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add duplicate for missing key */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP,
                                   "key1", 0, INI_VA_NOCHECK);
    fail_unless(ret == ENOENT, "Add str should fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_add_str_simple)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    const char *comments[] = { "// This is a test1", "// This is a test2",
                               NULL };

    char add_new_value_to_end[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n"
        "newkey = newvalue\n";

    char add_new_value_to_end_with_comment[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "newkey = newvalue\n";

    char add_new_value_to_front[] =
        "[zero]\n"
        "[one]\n"
        "newkey = newvalue\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    char add_new_value_to_front_with_comment[] =
        "[zero]\n"
        "[one]\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "newkey = newvalue\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    char add_new_value_after_key1[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "newkey = newvalue\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    char add_new_value_before_key2[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "newkey = newvalue\n"
        "key2 = value2a\n";

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Simple add new value to end of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_END, NULL,
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_end,
                               sizeof(add_new_value_to_end), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value with comment to end of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_END, NULL,
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_end_with_comment,
                               sizeof(add_new_value_to_end_with_comment),
                               in_cfg);
    ini_config_destroy(in_cfg);


    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Simple add new value to the begin of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FRONT, NULL,
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_front,
                               sizeof(add_new_value_to_front), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value with comment to the begin of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_FRONT, NULL,
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_front_with_comment,
                               sizeof(add_new_value_to_front_with_comment),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value after "key1" with index 0 */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_AFTER, "key1",
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_after_key1,
                               sizeof(add_new_value_after_key1),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value after "key1" with index 1 (index ignored) */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_AFTER, "key1",
                                   1, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_after_key1,
                               sizeof(add_new_value_after_key1),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value after "key1" with very big index (index ignored) */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_AFTER, "key1",
                                   1000, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_after_key1,
                               sizeof(add_new_value_after_key1),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value before "key2" */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_BEFORE, "key2",
                                   0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_before_key2,
                               sizeof(add_new_value_before_key2),
                               in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_add_str_duplicate)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    char add_first_duplicate[] =
        "[zero]\n"
        "[one]\n"
        "key0 = newvalue\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    char add_first_duplicate_for_multi[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = newvalue\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    char add_last_duplicate[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key1 = newvalue\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    char add_duplicate_with_index1[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "key1 = newvalue\n"
        "key1 = value1b\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    const char *comments[] = { "// This is a test1", "// This is a test2",
                               NULL };

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* cannot add key as the first duplicate for non-existing key */
    ret = ini_config_add_str_value(in_cfg, "one", "noexist", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_NOCHECK);
    fail_unless(ret == ENOENT, "Add duplicate key must fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* cannot add key as the last duplicate for non-existing key */
    ret = ini_config_add_str_value(in_cfg, "one", "noexist", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_LASTDUP,
                                   NULL, 0, INI_VA_NOCHECK);
    fail_unless(ret == ENOENT, "Add duplicate key must fail. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate value */
    ret = ini_config_add_str_value(in_cfg, "one", "key0", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_first_duplicate,
                               sizeof(add_first_duplicate), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate value with other_key (reference key is ignored) */
    ret = ini_config_add_str_value(in_cfg, "one", "key0", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   "key2", 0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_first_duplicate,
                               sizeof(add_first_duplicate), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate value with index (index is ignored) */
    ret = ini_config_add_str_value(in_cfg, "one", "key0", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   "key0", 1, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_first_duplicate,
                               sizeof(add_first_duplicate), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate value with multiple keys */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_first_duplicate_for_multi,
                               sizeof(add_first_duplicate_for_multi), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate as last */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_LASTDUP,
                                   NULL, 0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_last_duplicate,
                               sizeof(add_last_duplicate), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate with index 0 (the same as COL_DSP_FIRSTDUP) */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   NULL, 0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_first_duplicate_for_multi,
                               sizeof(add_first_duplicate_for_multi), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate with index and other_key (other_key is ignored) */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   "key0", 0, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_first_duplicate_for_multi,
                               sizeof(add_first_duplicate_for_multi), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate with big index (the same as COL_DSP_NDUP) */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   NULL, 100, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_last_duplicate,
                               sizeof(add_last_duplicate), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Add duplicate with index 1 */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   comments, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   NULL, 1, INI_VA_NOCHECK);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_duplicate_with_index1,
                               sizeof(add_duplicate_with_index1), in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_add_str_update_specific_value)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key0 = valuer0\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key1 = value1d\n"
        "key2 = value2\n"
        "key3 = value3\n";

    const char *comments[] = { "// This is a test1", "// This is a test2",
                               NULL };

    char modify_existing_value[] =
        "[zero]\n"
        "[one]\n"
        "key0 = valuer0\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "key1 = newvalue\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key1 = value1d\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char modify_existing_value_index[] =
        "[zero]\n"
        "[one]\n"
        "key0 = valuer0\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key1 = newvalue\n"
        "key2 = value2\n"
        "key3 = value3\n";

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* update of non-existing value fails */
    ret = ini_config_add_str_value(in_cfg, "one", "key4", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MOD);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_MOD will not add key for non-existing value
     * even with proper position flag. This is difference between
     * INI_VA_MOD and INI_VA_MODADD */
    ret = ini_config_add_str_value(in_cfg, "one", "key1.x", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_BEFORE, "key2",
                                   0, INI_VA_MOD);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data,
                               sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + update of non-existing value fails */
    ret = ini_config_add_str_value(in_cfg, "one", "key4", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MOD_E);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add will update existing value */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MOD);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value,
                               sizeof(modify_existing_value), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + add will update existing value */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MOD_E);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value,
                               sizeof(modify_existing_value), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add will update existing value using index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   3, INI_VA_MOD);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value_index,
                               sizeof(modify_existing_value_index), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + add will update existing value using index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   3, INI_VA_MOD_E);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value_index,
                               sizeof(modify_existing_value_index), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add will update existing value using big index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   1000, INI_VA_MOD);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value_index,
                               sizeof(modify_existing_value_index), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + add will NOT update existing value using big index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   1000, INI_VA_MOD_E);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data,
                               sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_add_str_update_MODADD)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key0 = valuer0\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key1 = value1d\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char modify_add_non_existing_value[] =
        "[zero]\n"
        "[one]\n"
        "key0 = valuer0\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key1 = value1d\n"
        "key1.x = newvalue\n"
        "key2 = value2\n"
        "key3 = value3\n";

    const char *comments[] = { "// This is a test1", "// This is a test2",
                               NULL };

    char modify_existing_value[] =
        "[zero]\n"
        "[one]\n"
        "key0 = valuer0\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "key1 = newvalue\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key1 = value1d\n"
        "key2 = value2\n"
        "key3 = value3\n";

    char modify_existing_value_index[] =
        "[zero]\n"
        "[one]\n"
        "key0 = valuer0\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key1 = value1c\n"
        "key1 = newvalue\n"
        "key2 = value2\n"
        "key3 = value3\n";

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* update of non-existing value fails with wrong position */
    ret = ini_config_add_str_value(in_cfg, "one", "key4", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MODADD);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_MODADD will add key for non-existing value*/
    ret = ini_config_add_str_value(in_cfg, "one", "key1.x", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_BEFORE, "key2",
                                   0, INI_VA_MODADD);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_add_non_existing_value,
                               sizeof(modify_add_non_existing_value), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + update of non-existing value fails */
    ret = ini_config_add_str_value(in_cfg, "one", "key4", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MODADD_E);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add will update existing value */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MODADD);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value,
                               sizeof(modify_existing_value), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + add will update existing value */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   0, INI_VA_MODADD_E);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value,
                               sizeof(modify_existing_value), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add will update existing value using index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   3, INI_VA_MODADD);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value_index,
                               sizeof(modify_existing_value_index), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + add will update existing value using index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   3, INI_VA_MODADD_E);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value_index,
                               sizeof(modify_existing_value_index), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add will update existing value using big index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   1000, INI_VA_MODADD);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(modify_existing_value_index,
                               sizeof(modify_existing_value_index), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* strict flag + add will NOT update existing value using big index */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP, NULL,
                                   1000, INI_VA_MODADD_E);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data,
                               sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

/* INI_VA_CLEAN does not have effect to operation without duplicit key */
START_TEST(test_add_str_simple_clean)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    const char *comments[] = { "// This is a test1", "// This is a test2",
                               NULL };

    char add_new_value_to_end[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n"
        "newkey = newvalue\n";

    char add_new_value_to_end_with_comment[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "newkey = newvalue\n";

    char add_new_value_to_front[] =
        "[zero]\n"
        "[one]\n"
        "newkey = newvalue\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    char add_new_value_to_front_with_comment[] =
        "[zero]\n"
        "[one]\n"
        "// This is a test1\n"
        "// This is a test2\n"
        "newkey = newvalue\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    char add_new_value_after_key1[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "newkey = newvalue\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    char add_new_value_before_key2[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "newkey = newvalue\n"
        "key2 = value2a\n";

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Simple add new value to end of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_END, NULL,
                                   0, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_end,
                               sizeof(add_new_value_to_end), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value with comment to end of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_END, NULL,
                                   0, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_end_with_comment,
                               sizeof(add_new_value_to_end_with_comment),
                               in_cfg);
    ini_config_destroy(in_cfg);


    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* Simple add new value to the begin of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FRONT, NULL,
                                   0, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_front,
                               sizeof(add_new_value_to_front), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value with comment to the begin of section */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   comments, 2, WRAP_SIZE, COL_DSP_FRONT, NULL,
                                   0, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_to_front_with_comment,
                               sizeof(add_new_value_to_front_with_comment),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value after "key1" with index 0 */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_AFTER, "key1",
                                   0, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_after_key1,
                               sizeof(add_new_value_after_key1),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value after "key1" with index 1 (index ignored) */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_AFTER, "key1",
                                   1, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_after_key1,
                               sizeof(add_new_value_after_key1),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value after "key1" with very big index (index ignored) */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_AFTER, "key1",
                                   1000, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_after_key1,
                               sizeof(add_new_value_after_key1),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* add new value before "key2" */
    ret = ini_config_add_str_value(in_cfg, "one", "newkey", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_BEFORE, "key2",
                                   0, INI_VA_CLEAN);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_new_value_before_key2,
                               sizeof(add_new_value_before_key2),
                               in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_add_str_duplicate_error)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key2 = value2a\n";

    char add_non_existing_value[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "key1.x = newvalue\n"
        "key2 = value2a\n";

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_DUPERROR will return EEXIST for duplicit key
     * and COL_DSP_FIRSTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key0", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_DUPERROR);
    fail_unless(ret == EEXIST, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_DUPERROR will return EEXIST for duplicit key
     * and COL_DSP_FIRSTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_DUPERROR);
    fail_unless(ret == EEXIST, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_DUPERROR will return EEXIST for duplicit key
     * and COL_DSP_FIRSTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key2", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_DUPERROR);
    fail_unless(ret == EEXIST, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_DUPERROR will return EEXIST for duplicit key
     * and COL_DSP_LASTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_LASTDUP,
                                   NULL, 0, INI_VA_DUPERROR);
    fail_unless(ret == EEXIST, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_DUPERROR will return EEXIST for duplicit key
     * and COL_DSP_NDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   NULL, 0, INI_VA_DUPERROR);
    fail_unless(ret == EEXIST, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_DUPERROR will return EEXIST for duplicit key
     * and COL_DSP_NDUP will return error due to nonexisting duplicate
     * (index and other_key are ignored.
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   "key1", 1, INI_VA_DUPERROR);
    fail_unless(ret == EEXIST, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_DUPERROR will return EEXIST for duplicit key
     * and COL_DSP_NDUP will return error due to nonexisting duplicate
     * big index is ignored.
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   NULL, 100, INI_VA_DUPERROR);
    fail_unless(ret == EEXIST, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* entry will be added with INI_VA_DUPERROR and non-existing key. */
    ret = ini_config_add_str_value(in_cfg, "one", "key1.x", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_BEFORE, "key2",
                                   0, INI_VA_DUPERROR);
    fail_unless(ret == EOK, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(add_non_existing_value,
                               sizeof(add_non_existing_value), in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

START_TEST(test_add_str_duplicate_clean)
{
    int ret = EOK;
    struct ini_cfgobj *in_cfg;

    char input_data[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    char only_removed_key[] =
        "[zero]\n"
        "[one]\n"
        "key1 = value1a\n"
        "key1 = value1b\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    char only_removed_all_duplicates[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "// This is a test1\n"
        "key2 = value2a\n";

    char only_removed_key_with_comment[] =
        "[zero]\n"
        "[one]\n"
        "key0 = value0a\n"
        "key1 = value1a\n"
        "key1 = value1b\n";

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_CLEAN will remove duplicit key
     * and COL_DSP_FIRSTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key0", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_CLEAN);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(only_removed_key, sizeof(only_removed_key),
                               in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_CLEAN will remove app duplicit keys
     * and COL_DSP_FIRSTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_CLEAN);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(only_removed_all_duplicates,
                               sizeof(only_removed_all_duplicates), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_CLEAN will remove app duplicit keys
     * and COL_DSP_FIRSTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key2", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_FIRSTDUP,
                                   NULL, 0, INI_VA_CLEAN);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(only_removed_key_with_comment,
                               sizeof(only_removed_key_with_comment), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_CLEAN will remove all duplicit keys
     * and COL_DSP_LASTDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_LASTDUP,
                                   NULL, 0, INI_VA_CLEAN);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(only_removed_all_duplicates,
                               sizeof(only_removed_all_duplicates), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_CLEAN will remove all duplicit keys
     * and COL_DSP_NDUP will return error due to nonexisting duplicate
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   NULL, 0, INI_VA_CLEAN);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(only_removed_all_duplicates,
                               sizeof(only_removed_all_duplicates), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_CLEAN will remove all duplicit keys
     * and COL_DSP_NDUP will return error due to nonexisting duplicate
     * (index and other_key are ignored.
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   "key1", 1, INI_VA_CLEAN);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(only_removed_all_duplicates,
                               sizeof(only_removed_all_duplicates), in_cfg);
    ini_config_destroy(in_cfg);

    in_cfg = get_ini_config_from_str(input_data, sizeof(input_data));
    assert_configuration_equal(input_data, sizeof(input_data), in_cfg);

    /* INI_VA_CLEAN will remove all duplicit keys
     * and COL_DSP_NDUP will return error due to nonexisting duplicate
     * big index is ignored.
     */
    ret = ini_config_add_str_value(in_cfg, "one", "key1", "newvalue",
                                   NULL, 0, WRAP_SIZE, COL_DSP_NDUP,
                                   NULL, 100, INI_VA_CLEAN);
    fail_unless(ret == ENOENT, "Failed to add str. Error: %d", ret);
    assert_configuration_equal(only_removed_all_duplicates,
                               sizeof(only_removed_all_duplicates), in_cfg);
    ini_config_destroy(in_cfg);
}
END_TEST

static Suite *ini_configmod_utils_suite(void)
{
    Suite *s = suite_create("ini_configmod");

    TCase *tc_delete_properties = tcase_create("delete_properties");
    tcase_add_test(tc_delete_properties, test_delete_value_wrong_arguments);
    tcase_add_test(tc_delete_properties, test_delete_value);

    suite_add_tcase(s, tc_delete_properties);

    TCase *tc_update_comments = tcase_create("update_comments");
    tcase_add_test(tc_update_comments, test_update_comments_wrong_arguments);
    tcase_add_test(tc_update_comments, test_update_comments);

    suite_add_tcase(s, tc_update_comments);

    TCase *tc_add_string = tcase_create("add_string");
    tcase_add_test(tc_add_string, test_add_str_wrong_arguments);
    tcase_add_test(tc_add_string, test_add_str_simple);
    tcase_add_test(tc_add_string, test_add_str_duplicate);
    tcase_add_test(tc_add_string, test_add_str_update_specific_value);
    tcase_add_test(tc_add_string, test_add_str_update_MODADD);
    tcase_add_test(tc_add_string, test_add_str_simple_clean);
    tcase_add_test(tc_add_string, test_add_str_duplicate_clean);
    tcase_add_test(tc_add_string, test_add_str_duplicate_error);

    suite_add_tcase(s, tc_add_string);

    return s;
}

int main(void)
{
    int number_failed;

    Suite *s = ini_configmod_utils_suite();
    SRunner *sr = srunner_create(s);
    /* If CK_VERBOSITY is set, use that, otherwise it defaults to CK_NORMAL */
    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
