/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include "toml.h"

void print_table(const TomlTable *table);
void print_value(const TomlValue *value);

void print_array(const TomlArray *array)
{
    printf("[");
    for (size_t i = 0; i < array->len; i++) {
        if (i > 0) {
            printf(", ");
        }
        print_value(array->elements[i]);
    }
    printf("]");
}

void print_value(const TomlValue *value)
{
    switch (value->type) {
        case TOML_TABLE:
            print_table(value->value.table);
            break;
        case TOML_ARRAY:
            print_array(value->value.array);
            break;
        case TOML_STRING:
            printf("\"%s\"", value->value.string->str);
            break;
        case TOML_INTEGER:
            printf("%" PRId64, value->value.integer);
            break;
        case TOML_FLOAT:
            printf("%f", value->value.float_);
            break;
        case TOML_DATETIME:
            printf("(datetime)");
            break;
        case TOML_BOOLEAN:
            printf("%s", value->value.boolean ? "true" : "false");
            break;
    }
}

void print_keyval(const TomlKeyValue *keyval)
{
    printf("\"%s\": ", keyval->key->str);
    print_value(keyval->value);
}

void print_table(const TomlTable *table)
{
    TomlTableIter it = toml_table_iter_new((TomlTable *)table);

    printf("{");
    size_t i = 0;
    while (toml_table_iter_has_next(&it)) {
        TomlKeyValue *keyval = toml_table_iter_get(&it);

        if (i > 0) {
            printf(", ");
        }
        print_keyval(keyval);

        toml_table_iter_next(&it);
        i++;
    }
    printf("}");
}

int test_run(const char *filename)
{
    TomlTable *table = NULL;
    int rc = 0;

    table = toml_load_filename(filename);
    if (table == NULL)
        goto cleanup;

    print_table(table);
    printf("\n");

cleanup:
    toml_table_free(table);

    if (toml_err()->code != TOML_OK) {
        fprintf(stderr, "%s\n", toml_err()->message);
        rc = (int)toml_err()->code;
    }
    toml_err_clear();
    return rc;
}

int main(void)
{
    static const char *const filenames[] = {
        /* should parse */
        PROJECT_SOURCE_DIR "/tests/key-values.toml",
        PROJECT_SOURCE_DIR "/tests/complex-structure.toml",
        PROJECT_SOURCE_DIR "/tests/long_config.toml",

        /* should not parse */

        /* tests from https://github.com/toml-lang/toml */
        PROJECT_SOURCE_DIR "/tests/example.toml",
        PROJECT_SOURCE_DIR "/tests/fruit.toml",
        PROJECT_SOURCE_DIR "/tests/hard_example.toml",
        PROJECT_SOURCE_DIR "/tests/hard_example_unicode.toml"
    };

    int total_tests = sizeof(filenames) / sizeof(char *);
    int num_passed = 0;
    int num_failed = 0;

    for (int i = 0; i < total_tests; i++) {
        int rc = test_run(filenames[i]);
        if (rc == 0) {
            printf("test %d success\n", i);
            num_passed++;
        } else {
            printf("test %d returned %d\n", i, rc);
            num_failed++;
        }
    }

    printf("total %d tests, %d passed, %d failed\n",
           total_tests, num_passed, num_failed);

    return num_failed;
}
