/*
    Authors:
        Michal Zidek <mzidek@redhat.com>

    Copyright (C) 2016 Red Hat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <check.h>

/* #define TRACE_LEVEL 7 */
#define TRACE_HOME
#include "dhash.h"

#define HTABLE_SIZE 128

int verbose = 0;

/* There must be no warnings generated during this test
 * without having to cast the key value. */
START_TEST(test_key_const_string)
{
    hash_table_t *htable;
    int ret;
    hash_value_t ret_val;
    hash_value_t enter_val1;
    hash_value_t enter_val2;
    hash_key_t key;

    enter_val1.type = HASH_VALUE_INT;
    enter_val1.i = 1;
    enter_val2.type = HASH_VALUE_INT;
    enter_val2.i = 2;
    key.type = HASH_KEY_CONST_STRING;
    key.c_str = "constant";

    ret = hash_create(HTABLE_SIZE, &htable, NULL, NULL);
    fail_unless(ret == 0);

    /* The table is empty, lookup should return error */
    ret = hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    /* Deleting with non-existing key should return error */
    ret = hash_delete(htable, &key);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    ret = hash_enter(htable, &key, &enter_val1);
    fail_unless(ret == 0);

    hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == 0);
    fail_unless(ret_val.i == 1);

    /* Overwrite the entry */
    ret = hash_enter(htable, &key, &enter_val2);
    fail_unless(ret == 0);

    hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == 0);
    fail_unless(ret_val.i == 2);

    ret = hash_delete(htable, &key);
    fail_unless(ret == 0);

    /* Delete again with the same key */
    ret = hash_delete(htable, &key);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    ret = hash_destroy(htable);
    fail_unless(ret == 0);
}
END_TEST

START_TEST(test_key_string)
{
    hash_table_t *htable;
    int ret;
    hash_value_t ret_val;
    hash_value_t enter_val1;
    hash_value_t enter_val2;
    hash_key_t key;
    char str[] = "non_constant";

    enter_val1.type = HASH_VALUE_INT;
    enter_val1.i = 1;
    enter_val2.type = HASH_VALUE_INT;
    enter_val2.i = 2;
    key.type = HASH_KEY_STRING;
    key.str = str;

    ret = hash_create(HTABLE_SIZE, &htable, NULL, NULL);
    fail_unless(ret == 0);

    /* The table is empty, lookup should return error */
    ret = hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    /* Deleting with non-existing key should return error */
    ret = hash_delete(htable, &key);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    ret = hash_enter(htable, &key, &enter_val1);
    fail_unless(ret == 0);

    hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == 0);
    fail_unless(ret_val.i == 1);

    /* Overwrite the entry */
    ret = hash_enter(htable, &key, &enter_val2);
    fail_unless(ret == 0);

    hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == 0);
    fail_unless(ret_val.i == 2);

    ret = hash_delete(htable, &key);
    fail_unless(ret == 0);

    /* Delete again with the same key */
    ret = hash_delete(htable, &key);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);


    ret = hash_destroy(htable);
    fail_unless(ret == 0);
}
END_TEST

START_TEST(test_key_ulong)
{
    hash_table_t *htable;
    int ret;
    hash_value_t ret_val;
    hash_value_t enter_val1;
    hash_value_t enter_val2;
    hash_key_t key;

    enter_val1.type = HASH_VALUE_INT;
    enter_val1.i = 1;
    enter_val2.type = HASH_VALUE_INT;
    enter_val2.i = 2;
    key.type = HASH_KEY_ULONG;
    key.ul = 68ul;

    ret = hash_create(HTABLE_SIZE, &htable, NULL, NULL);
    fail_unless(ret == 0);

    /* The table is empty, lookup should return error */
    ret = hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    /* Deleting with non-existing key should return error */
    ret = hash_delete(htable, &key);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    ret = hash_enter(htable, &key, &enter_val1);
    fail_unless(ret == 0);

    hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == 0);
    fail_unless(ret_val.i == 1);

    /* Overwrite the entry */
    ret = hash_enter(htable, &key, &enter_val2);
    fail_unless(ret == 0);

    hash_lookup(htable, &key, &ret_val);
    fail_unless(ret == 0);
    fail_unless(ret_val.i == 2);

    ret = hash_delete(htable, &key);
    fail_unless(ret == 0);

    /* Delete again with the same key */
    ret = hash_delete(htable, &key);
    fail_unless(ret == HASH_ERROR_KEY_NOT_FOUND);

    ret = hash_destroy(htable);
    fail_unless(ret == 0);
}
END_TEST

static Suite *dhash_suite(void)
{
    Suite *s = suite_create("");

    TCase *tc_basic = tcase_create("dhash API tests");
    tcase_add_test(tc_basic, test_key_const_string);
    tcase_add_test(tc_basic, test_key_string);
    tcase_add_test(tc_basic, test_key_ulong);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    int number_failed;

    Suite *s = dhash_suite();
    SRunner *sr = srunner_create(s);
    /* If CK_VERBOSITY is set, use that, otherwise it defaults to CK_NORMAL */
    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
