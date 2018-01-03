/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include "toml.h"

void print_table(const TomlTable *table, TomlErr *error);
void print_value(const TomlValue *value, TomlErr *error);

void print_array(const TomlArray *array, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  printf("[");
  for (size_t i = 0; i < array->len; i++) {
    if (i > 0) {
      printf(", ");
    }
    print_value(array->elements[i], &err);
    if (err.code != TOML_OK) goto cleanup;
  }
  printf("]");

cleanup:
  toml_err_move(error, &err);
}

void print_value(const TomlValue *value, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  switch (value->type) {
  case TOML_TABLE:
    print_table(value->value.table, &err);
    if (err.code != TOML_OK) goto cleanup;
    break;
  case TOML_ARRAY:
    print_array(value->value.array, &err);
    if (err.code != TOML_OK) goto cleanup;
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

cleanup:
  toml_err_move(error, &err);
}

void print_keyval(const TomlKeyValue *keyval, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  printf("\"%s\": ", keyval->key->str);

  print_value(keyval->value, &err);
  if (err.code != TOML_OK) {
    toml_err_move(error, &err);
    return;
  }
}

void print_table(const TomlTable *table, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlTableIter *it = NULL;

  it = toml_table_iter_new((TomlTable *)table, &err);
  if (err.code != TOML_OK) goto cleanup;

  printf("{");
  size_t i = 0;
  while (toml_table_iter_has_next(it)) {
    TomlKeyValue *keyval = toml_table_iter_get(it);

    if (i > 0) {
      printf(", ");
    }
    print_keyval(keyval, &err);
    if (err.code != TOML_OK) goto cleanup;

    toml_table_iter_next(it);
    i++;
  }
  printf("}");

cleanup:
  toml_table_iter_free(it);
  toml_err_move(error, &err);
}

int test_run(const char *filename)
{
  TomlErr err = TOML_ERR_INIT;
  TomlTable *table = NULL;
  int rc = 0;

  table = toml_load_filename(filename, &err);
  if (err.code != TOML_OK) goto cleanup;

  print_table(table, &err);
  printf("\n");

cleanup:
  toml_table_free(table);

  if (err.code != TOML_OK) {
    fprintf(stderr, "%s\n", err.message);
  }
  rc = err.code;
  toml_clear_err(&err);
  return rc;
}

int main(void)
{
  static const char *const filenames[] = {
    "example.toml",
    "fruit.toml",
    "hard_example.toml",
    "hard_example_unicode.toml"
  };

  int total_tests = sizeof(filenames) / sizeof(char *);
  int num_success = 0;
  int num_fail = 0;

  for (int i = 0; i < total_tests; i++) {
    int rc = test_run(filenames[i]);
    if (rc == 0) {
      printf("test %d success\n", i);
      num_success++;
    } else {
      printf("test %d returned %d\n", i, rc);
      num_fail++;
    }
  }

  printf("total %d tests, %d success, %d fail\n",
         total_tests, num_success, num_fail);

  if (num_fail > 0) {
    return -1;
  } else {
    return 0;
  }
}
