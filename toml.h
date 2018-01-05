/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TOML_H__
#define __TOML_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

enum {
  TOML_OK,
  TOML_ERR,
  TOML_ERR_IO,
  TOML_ERR_NOMEM,
  TOML_ERR_SYNTAX,
};

typedef struct {
  int code;
  char *message;
  bool _is_literal;
} TomlErr;

typedef struct {
  char      *str;
  size_t    len;
  size_t    _capacity;
} TomlString;

typedef struct _TomlValue TomlValue;

typedef struct {
  TomlValue **elements;
  size_t    len;
  size_t    _capacity;
} TomlArray;

typedef struct {
  TomlString    *key;
  TomlValue     *value;
} TomlKeyValue;

typedef struct _TomlTable TomlTable;
typedef struct _TomlTableIter TomlTableIter;

typedef struct {
  struct tm     tm;
  double        sec_frac;
} TomlDateTime;

typedef enum {
  TOML_TABLE,
  TOML_ARRAY,
  TOML_STRING,
  TOML_INTEGER,
  TOML_FLOAT,
  TOML_DATETIME,
  TOML_BOOLEAN,
} TomlType;

struct _TomlValue {
  TomlType  type;
  union {
    TomlTable       *table;
    TomlArray       *array;
    TomlString      *string;
    int64_t         integer;
    double          float_;
    TomlDateTime    *datetime;
    bool            boolean;
  } value;
};

char *toml_strdup(const char *str);
char *toml_strndup(const char *str, size_t n);
int toml_vasprintf(char **str, const char *format, va_list args);
int toml_asprintf(char **str, const char *format, ...);

#define TOML_ERR_INIT {TOML_OK, NULL, false}

void toml_err_init(TomlErr *err);
void toml_clear_err(TomlErr *err);
void toml_err_move(TomlErr *to, TomlErr *from);
void toml_set_err_v(TomlErr *err, int code, const char *format, va_list args);
void toml_set_err(TomlErr *err, int code, const char *format, ...);
void toml_set_err_literal(TomlErr *err, int code, const char *message);

TomlString *toml_string_new(TomlErr *err);
TomlString *toml_string_new_string(const char *str, TomlErr *err);
TomlString *toml_string_new_nstring(const char *str, size_t len, TomlErr *err);
void toml_string_append_char(TomlString *self, char ch, TomlErr *err);
void toml_string_append_string(TomlString *self, const char *str, TomlErr *err);
void toml_string_append_nstring(TomlString *self, const char *str, size_t len, TomlErr *err);
TomlString *toml_string_copy(const TomlString *self, TomlErr *err);
void toml_string_free(TomlString *self);
bool toml_string_equals(const TomlString *self, const TomlString *other);

TomlTable *toml_table_new(TomlErr *err);
void toml_table_free(TomlTable *self);

void toml_table_set_by_string(TomlTable *self, TomlString *key,
                              TomlValue *value, TomlErr *err);
TomlValue *toml_table_get_by_string(const TomlTable *self, const TomlString *key);
void toml_table_set(TomlTable *self, const char *key, TomlValue *value, TomlErr *err);
void toml_table_setn(TomlTable *self, const char *key, size_t key_len,
                      TomlValue *value, TomlErr *err);
TomlValue *toml_table_get(const TomlTable *self, const char *key);
TomlValue *toml_table_getn(const TomlTable *self, const char *key, size_t key_len);

TomlTableIter *toml_table_iter_new(TomlTable *table, TomlErr *err);
void toml_table_iter_free(TomlTableIter *self);
TomlKeyValue *toml_table_iter_get(TomlTableIter *self);
bool toml_table_iter_has_next(TomlTableIter *self);
void toml_table_iter_next(TomlTableIter *self);

TomlArray *toml_array_new(TomlErr *err);
void toml_array_free(TomlArray *self);
void toml_array_append(TomlArray *self, TomlValue *value, TomlErr *err);

TomlValue *toml_value_new(TomlType type, TomlErr *err);
TomlValue *toml_value_new_string(const char *str, TomlErr *err);
TomlValue *toml_value_new_nstring(const char *str, size_t len, TomlErr *err);
TomlValue *toml_value_new_table(TomlErr *err);
TomlValue *toml_value_new_array(TomlErr *err);
TomlValue *toml_value_new_integer(int64_t integer, TomlErr *err);
TomlValue *toml_value_new_float(double flt, TomlErr *err);
TomlValue *toml_value_new_datetime(TomlErr *err);
TomlValue *toml_value_new_boolean(bool boolean, TomlErr *err);
void toml_value_free(TomlValue *self);

TomlTable *toml_load_string(const char *str, TomlErr *err);
TomlTable *toml_load_nstring(const char *str, size_t len, TomlErr *err);
TomlTable *toml_load_file(FILE *file, TomlErr *err);
TomlTable *toml_load_filename(const char *filename, TomlErr *err);

/* TODO: implement dump functions
char *toml_dump_string(const TomlTable *self, TomlErr *err);
TomlString *toml_dump_nstring(const TomlTable *self, TomlErr *err);
void toml_dump_file(const TomlTable *self, FILE *file, TomlErr *err);
*/

#ifdef __cplusplus
}
#endif

#endif /* end of include guard: __TOML_H__ */
