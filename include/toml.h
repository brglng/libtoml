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
#include <time.h>

#if defined(__cplusplus) && __cplusplus >= 201103L
#define TOML_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define TOML_THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define TOML_THREAD_LOCAL __declspec(thread)
#else
#define TOML_THREAD_LOCAL __thread
#endif

#if !defined(_MSC_VER) || _MSC_VER >= 1800
#define TOML_INLINE static inline
#define TOML_CONST const
#define TOML_RESTRICT restrict
#else
#define TOML_INLINE static __inline
#define TOML_CONST
#define TOML_RESTRICT __restrict
#endif

#define TOML_FALSE  0
#define TOML_TRUE   1

typedef enum {
    TOML_OK,
    TOML_ERR,
    TOML_ERR_OS,
    TOML_ERR_NOMEM,
    TOML_ERR_SYNTAX,
    TOML_ERR_UNICODE
} TomlErrCode;

typedef struct {
    TomlErrCode     code;
    char*           message;
    int             _is_literal;
} TomlErr;

typedef struct {
    char*   str;
    size_t  len;
    size_t  _capacity;
} TomlString;

typedef struct _TomlValue TomlValue;

typedef struct {
    TomlValue**     elements;
    size_t          len;
    size_t          _capacity;
} TomlArray;

typedef struct _TomlKeyValue TomlKeyValue;

typedef struct {
    size_t          _capacity;
    size_t          len;
    TomlKeyValue*   _keyvals;
} TomlTable;

typedef struct {
    TomlTable*      _table;
    TomlKeyValue*   _keyval;
} TomlTableIter;

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
        TomlTable*      table;
        TomlArray*      array;
        TomlString*     string;
#if defined(_MSC_VER) || defined(__APPLE__)
        long long       integer;
#else
        long            integer;
#endif
        double          float_;
        struct tm       datetime;
        int             boolean;
    } value;
};

struct _TomlKeyValue {
    TomlString*     key;
    TomlValue*      value;
};

typedef struct {
    void*   (*aligned_alloc)(void *context, size_t alignment, size_t size);
    void*   (*realloc)(void *context, void *p, size_t size);
    void    (*free)(void *context, void *p);
} TomlAllocFuncs;

void toml_set_allocator(void *context, TOML_CONST TomlAllocFuncs *funcs);

void* toml_aligned_alloc(size_t alignment, size_t size);
void* toml_realloc(void *p, size_t size);
void toml_free(void *p);

#ifdef __cplusplus
#define TOML_ALIGNOF(type) alignof(type)
#else
#if __STDC_VERSION__ >= 201112L
#define TOML_ALIGNOF(type) _Alignof(type)
#else
#define TOML_ALIGNOF(type) offsetof(struct { char c; type d; }, d)
#endif
#endif

#define _TOML_NEW0(_type) toml_aligned_alloc(TOML_ALIGNOF(_type), sizeof(_type))
#define _TOML_NEW1(_type, _count) toml_aligned_alloc(TOML_ALIGNOF(_type), sizeof(_type) * (_count))
#define _TOML_NEW(_type, _0, _macro_name, ...) _macro_name
#define TOML_NEW(_type, ...) ((_type*)_TOML_NEW(_type, ##__VA_ARGS__, _TOML_NEW1, _TOML_NEW0)(_type, ##__VA_ARGS__))

char* toml_strdup(TOML_CONST char *str);
char* toml_strndup(TOML_CONST char *str, size_t n);
int toml_vasprintf(char **str, TOML_CONST char *format, va_list args);
int toml_asprintf(char **str, TOML_CONST char *format, ...);

TOML_CONST TomlErr* toml_err(void);
void toml_err_clear(void);

TomlString* toml_string_new(void);
TomlString* toml_string_from_str(TOML_CONST char *str);
TomlString* toml_string_from_nstr(TOML_CONST char *str, size_t len);
void toml_string_append_char(TomlString *self, char ch);
void toml_string_append_str(TomlString *self, TOML_CONST char *str);
void toml_string_append_nstr(TomlString *self, TOML_CONST char *str, size_t len);
TomlString* toml_string_clone(TOML_CONST TomlString *self);
void toml_string_free(TomlString *self);
int toml_string_equals(TOML_CONST TomlString *self, TOML_CONST TomlString *other);

TomlTable* toml_table_new(void);
void toml_table_free(TomlTable *self);

void toml_table_set_by_string(TomlTable *self, TomlString *key, TomlValue *value);
TomlValue *toml_table_get_by_string(TOML_CONST TomlTable *self, TOML_CONST TomlString *key);
void toml_table_set(TomlTable *self, TOML_CONST char *key, TomlValue *value);
void toml_table_setn(TomlTable *self, TOML_CONST char *key, size_t key_len, TomlValue *value);
TomlValue* toml_table_get(TOML_CONST TomlTable *self, TOML_CONST char *key);
TomlTable* toml_table_get_as_table(TOML_CONST TomlTable *self, TOML_CONST char *key);
TomlArray* toml_table_get_as_array(TOML_CONST TomlTable *self, TOML_CONST char *key);
TomlString* toml_table_get_as_string(TOML_CONST TomlTable *self, TOML_CONST char *key);
#if defined(_MSC_VER) || defined(__APPLE__)
long long toml_table_get_as_integer(TOML_CONST TomlTable *self, TOML_CONST char *key);
#else
long toml_table_get_as_integer(TOML_CONST TomlTable *self, TOML_CONST char *key);
#endif
double toml_table_get_as_float(TOML_CONST TomlTable *self, TOML_CONST char *key);
const struct tm* toml_table_get_as_datetime(TOML_CONST TomlTable *self, TOML_CONST char *key);
int toml_table_get_as_boolean(TOML_CONST TomlTable *self, TOML_CONST char *key);
TomlValue* toml_table_getn(TOML_CONST TomlTable *self, TOML_CONST char *key, size_t key_len);

TomlTableIter toml_table_iter_new(TomlTable *table);
TomlKeyValue* toml_table_iter_get(TomlTableIter *self);
int toml_table_iter_has_next(TomlTableIter *self);
void toml_table_iter_next(TomlTableIter *self);

TomlArray* toml_array_new(void);
void toml_array_free(TomlArray *self);
void toml_array_append(TomlArray *self, TomlValue *value);

TomlValue* toml_value_new(TomlType type);
TomlValue* toml_value_new_string(TomlType type);
TomlValue* toml_value_new_table(void);
TomlValue* toml_value_new_array(void);
#if defined(_MSC_VER) || defined(__APPLE__)
TomlValue *toml_value_new_integer(long long integer);
#else
TomlValue *toml_value_new_integer(long integer);
#endif
TomlValue* toml_value_new_float(double flt);
TomlValue* toml_value_new_datetime(void);
TomlValue* toml_value_new_boolean(int boolean);
TomlValue* toml_value_from_str(TOML_CONST char *str);
TomlValue* toml_value_from_nstr(TOML_CONST char *str, size_t len);
void toml_value_free(TomlValue *self);

TomlTable* toml_load_str(TOML_CONST char *str);
TomlTable* toml_load_nstr(TOML_CONST char *str, size_t len);
TomlTable* toml_load_file(FILE *file);
TomlTable* toml_load_filename(TOML_CONST char *filename);

/* TODO: implement dump functions
char *toml_dump_str(TOML_CONST TomlTable *self, TomlErr *err);
TomlString *toml_dump_nstr(TOML_CONST TomlTable *self, TomlErr *err);
void toml_dump_file(TOML_CONST TomlTable *self, FILE *file, TomlErr *err);
*/

#ifdef __cplusplus
}
#endif

#endif /* end of include guard: __TOML_H__ */
