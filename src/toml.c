/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include "toml.h"

static TOML_THREAD_LOCAL TomlErr g_err = {TOML_OK, (char*)"", TOML_TRUE};

static void* toml_default_malloc(void *context, size_t size)
{
    (void)context;
    void *p = malloc(size);
    assert(p != NULL);
    return p;
}

static void* toml_default_realloc(void *context, void *p, size_t size)
{
    (void)context;
    void *ptr = realloc(p, size);
    assert(ptr != NULL);
    return ptr;
}

static void toml_default_free(void *context, void *p)
{
    (void)context;
    free(p);
}

static TomlAllocFuncs g_default_alloc_funcs = {
    &toml_default_malloc,
    &toml_default_realloc,
    &toml_default_free
};

static void *g_alloc_context = NULL;
static const TomlAllocFuncs* g_alloc_funcs = &g_default_alloc_funcs;

void toml_set_allocator(void *context, TOML_CONST TomlAllocFuncs *funcs)
{
    g_alloc_context = context;
    g_alloc_funcs = funcs;
}

void* toml_malloc(size_t size)
{
    return g_alloc_funcs->malloc(g_alloc_context, size);
}

void* toml_realloc(void *p, size_t size)
{
    return g_alloc_funcs->realloc(g_alloc_context, p, size);
}

void toml_free(void *p)
{
    if (p != NULL) {
        g_alloc_funcs->free(g_alloc_context, p);
    }
}

char* toml_strdup(TOML_CONST char *str)
{
    size_t len = strlen(str) + 1;
    void *new = toml_malloc(len);
    if (new == NULL)
        return NULL;

    return memcpy(new, str, len);
}

char *toml_strndup(TOML_CONST char *str, size_t n)
{
    char *result = toml_malloc(n + 1);
    if (result == NULL)
        return NULL;

    result[n] = 0;
    return memcpy(result, str, n);
}

int toml_vasprintf(char **str, TOML_CONST char *format, va_list args)
{
    int size = 0;

    va_list args_copy;
    va_copy(args_copy, args);
    size = vsnprintf(NULL, (size_t)size, format, args_copy);
    va_end(args_copy);

    if (size < 0) {
        return size;
    }

    *str = toml_malloc((size_t)size + 1);
    if (*str == NULL)
        return -1;

    return vsprintf(*str, format, args);
}

int toml_asprintf(char **str, TOML_CONST char *format, ...)
{
    va_list args;
    va_start(args, format);
    int size = toml_vasprintf(str, format, args);
    va_end(args);
    return size;
}

TOML_CONST TomlErr* toml_err(void)
{
    return &g_err;
}

void toml_err_clear(void)
{
    if (g_err.code != TOML_OK) {
        if (!g_err._is_literal) {
            toml_free(g_err.message);
        }
        g_err.code = TOML_OK;
        g_err.message = "";
        g_err._is_literal = TOML_TRUE;
    }
}

TOML_INLINE void toml_err_set(TomlErrCode code, TOML_CONST char *format, ...)
{
    assert(g_err.code == TOML_OK);
    va_list args;
    va_start(args, format);
    g_err.code = code;
    toml_vasprintf(&g_err.message, format, args);
    g_err._is_literal = TOML_FALSE;
    va_end(args);
}

TOML_INLINE void toml_err_set_literal(TomlErrCode code, TOML_CONST char *message)
{
    assert(g_err.code == TOML_OK);
    g_err.code = code;
    g_err.message = (char *)message;
    g_err._is_literal = TOML_TRUE;
}

TOML_INLINE size_t toml_roundup_pow_of_two_size_t(size_t x)
{
    size_t v = x;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
#if SIZE_MAX == 0xffff
    v |= v >> 8;
#elif SIZE_MAX == 0xffffffff
    v |= v >> 8;
    v |= v >> 16;
#elif SIZE_MAX == 0xffffffffffffffffll
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
#endif
    v++;
    return v;
}

TomlString *toml_string_new(void)
{
    TomlString *self = toml_malloc(sizeof(TomlString));
    self->str = NULL;
    self->len = 0;
    self->_capacity = 0;
    return self;
}

TomlString *toml_string_from_str(TOML_CONST char *str)
{
    TomlString *self = toml_string_new();
    toml_string_append_str(self, str);
    return self;
}

TomlString *toml_string_from_nstr(TOML_CONST char *str, size_t len)
{
    TomlString *self = toml_string_new();
    toml_string_append_nstr(self, str, len);
    return self;
}

TOML_INLINE void toml_string_expand_if_necessary(TomlString *self, size_t len_to_add)
{
    if (self->len + len_to_add + 1 > self->_capacity) {
        size_t new_capacity = toml_roundup_pow_of_two_size_t(self->len + len_to_add + 1);
        new_capacity = new_capacity >= 8 ? new_capacity : 8;
        self->str = toml_realloc(self->str, new_capacity);
        self->_capacity = new_capacity;
    }
}

void toml_string_append_char(TomlString *self, char ch)
{
    toml_string_expand_if_necessary(self, 1);
    self->str[self->len] = ch;
    self->str[self->len + 1] = 0;
    self->len++;
}

void toml_string_append_str(TomlString *self, TOML_CONST char *str)
{
    size_t len = strlen(str);
    toml_string_expand_if_necessary(self, len);
    memcpy(&self->str[self->len], str, len + 1);
    self->len += len;
}

void toml_string_append_nstr(TomlString *self, TOML_CONST char *str, size_t len)
{
    toml_string_expand_if_necessary(self, len);
    memcpy(&self->str[self->len], str, len);
    self->len += len;
    self->str[self->len] = 0;
}

void toml_string_free(TomlString *self)
{
    if (self != NULL) {
        free(self->str);
        free(self);
    }
}

TomlString *toml_string_clone(TOML_CONST TomlString *self)
{
    return toml_string_from_nstr(self->str, self->len);
}

int toml_string_equals(TOML_CONST TomlString *self, TOML_CONST TomlString *other)
{
    if (self == other) {
        return TOML_TRUE;
    }

    if (self->len != other->len) {
        return TOML_FALSE;
    }

    if (self->str == other->str) {
        return TOML_TRUE;
    }

    for (size_t i = 0; i < self->len; i++) {
        if (self->str[i] != other->str[i]) {
            return TOML_FALSE;
        }
    }

    return TOML_TRUE;
}

TomlTable *toml_table_new(void)
{
    TomlTable *self = toml_malloc(sizeof(TomlTable));
    self->_capacity = 0;
    self->_keyvals = NULL;
    self->len = 0;
    return self;
}

void toml_table_free(TomlTable *self)
{
    if (self != NULL) {
        for (size_t i = 0; i < self->len; i++) {
            toml_string_free(self->_keyvals[i].key);
            toml_value_free(self->_keyvals[i].value);
        }
        free(self->_keyvals);
        free(self);
    }
}

void toml_table_expand_if_necessary(TomlTable *self)
{
    if (self->len + 1 > self->_capacity) {
        size_t new_capacity = self->_capacity > 0 ? self->_capacity * 2 : 8;
        void *p = toml_realloc(self->_keyvals, sizeof(TomlKeyValue) * new_capacity);
        self->_keyvals = p;
        self->_capacity = new_capacity;
    }
}

void toml_table_set_by_string(TomlTable *self, TomlString *key, TomlValue *value)
{
    TomlValue **slot = NULL;
    for (size_t i = 0; i < self->len; i++) {
        if (toml_string_equals(self->_keyvals[i].key, key)) {
            slot = &self->_keyvals[i].value;
        }
    }

    if (slot == NULL) {
        toml_table_expand_if_necessary(self);
        self->_keyvals[self->len].key = key;
        self->_keyvals[self->len].value = value;
        self->len++;
    } else {
        *slot = value;
    }
}

TomlValue *toml_table_get_by_string(TOML_CONST TomlTable *self, TOML_CONST TomlString *key)
{
    TomlValue *value = NULL;
    for (size_t i = 0; i < self->len; i++) {
        if (toml_string_equals(self->_keyvals[i].key, key)) {
            value = self->_keyvals[i].value;
        }
    }
    return value;
}

TomlValue *toml_table_getn(TOML_CONST TomlTable *self, TOML_CONST char *key, size_t key_len)
{
    TomlString str = {(char *)key, key_len, 0};
    return toml_table_get_by_string(self, &str);
}

TomlValue *toml_table_get(TOML_CONST TomlTable *self, TOML_CONST char *key)
{
    return toml_table_getn(self, key, strlen(key));
}

TomlTable* toml_table_get_as_table(TOML_CONST TomlTable *self, TOML_CONST char *key)
{
    TomlValue *v = toml_table_get(self, key);
    assert(v != NULL);
    assert(v->type == TOML_TABLE);
    return v->value.table;
}

TomlArray* toml_table_get_as_array(TOML_CONST TomlTable *self, TOML_CONST char *key)
{
    TomlValue *v = toml_table_get(self, key);
    assert(v != NULL);
    assert(v->type == TOML_ARRAY);
    return v->value.array;
}

TomlString* toml_table_get_as_string(TOML_CONST TomlTable *self, TOML_CONST char *key)
{
    TomlValue *v = toml_table_get(self, key);
    assert(v != NULL);
    assert(v->type == TOML_STRING);
    return v->value.string;
}

#if defined(_MSC_VER) || defined(__APPLE__)
long long toml_table_get_as_integer(TOML_CONST TomlTable *self, TOML_CONST char *key)
#else
long toml_table_get_as_integer(TOML_CONST TomlTable *self, TOML_CONST char *key)
#endif
{
    TomlValue *v = toml_table_get(self, key);
    assert(v != NULL);
    assert(v->type == TOML_INTEGER);
    return v->value.integer;
}

double toml_table_get_as_float(TOML_CONST TomlTable *self, TOML_CONST char *key)
{
    TomlValue *v = toml_table_get(self, key);
    assert(v != NULL);
    assert(v->type == TOML_FLOAT);
    return v->value.float_;
}

const struct tm* toml_table_get_as_datetime(TOML_CONST TomlTable *self, TOML_CONST char *key)
{
    TomlValue *v = toml_table_get(self, key);
    assert(v != NULL);
    assert(v->type == TOML_DATETIME);
    return &v->value.datetime;
}

int toml_table_get_as_boolean(TOML_CONST TomlTable *self, TOML_CONST char *key)
{
    TomlValue *v = toml_table_get(self, key);
    assert(v != NULL);
    assert(v->type == TOML_BOOLEAN);
    return v->value.boolean;
}

void toml_table_setn(TomlTable *self, TOML_CONST char *key, size_t key_len, TomlValue *value)
{
    TomlString *str = toml_string_from_nstr(key, key_len);
    toml_table_set_by_string(self, str, value);
}

void toml_table_set(TomlTable *self, TOML_CONST char *key, TomlValue *value)
{
    toml_table_setn(self, key, strlen(key), value);
}

TomlTableIter toml_table_iter_new(TomlTable *table)
{
    TomlTableIter self = { table, table->_keyvals };
    return self;
}

TomlKeyValue* toml_table_iter_get(TomlTableIter *self)
{
    return self->_keyval;
}

int toml_table_iter_has_next(TomlTableIter *self)
{
    return self->_keyval != NULL;
}

void toml_table_iter_next(TomlTableIter *self)
{
    if (self->_keyval < self->_table->_keyvals + self->_table->len) {
        self->_keyval++;
    }

    if (self->_keyval >= self->_table->_keyvals + self->_table->len) {
        self->_keyval = NULL;
    }
}

TomlArray *toml_array_new(void)
{
    TomlArray *self = toml_malloc(sizeof(TomlArray));
    self->elements = NULL;
    self->len = 0;
    self->_capacity = 0;
    return self;
}

void toml_array_free(TomlArray *self)
{
    if (self != NULL) {
        for (size_t i = 0; i < self->len; i++) {
            toml_value_free(self->elements[i]);
        }
        free(self->elements);
        free(self);
    }
}

void toml_array_expand_if_necessary(TomlArray *self)
{
    if (self->len + 1 > self->_capacity) {
        size_t new_capacity = self->_capacity > 0 ? self->_capacity * 2 : 8;
        void *p = toml_realloc(self->elements, sizeof(TomlValue *) * new_capacity);
        self->elements = p;
        self->_capacity = new_capacity;
    }
}

void toml_array_append(TomlArray *self, TomlValue *value)
{
    toml_array_expand_if_necessary(self);
    self->elements[self->len++] = value;
}

TomlValue *toml_value_new(TomlType type)
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->type = type;
    switch (type) {
        case TOML_TABLE:
            self->value.table = NULL;
            break;
        case TOML_ARRAY:
            self->value.array = NULL;
            break;
        case TOML_STRING:     
            self->value.string = NULL;      
            break;
        case TOML_INTEGER:    
            self->value.integer = 0;        
            break;
        case TOML_FLOAT:      
            self->value.float_ = 0.0;       
            break;
        case TOML_BOOLEAN:    
            self->value.boolean = TOML_FALSE;    
            break;
        case TOML_DATETIME:
            memset(&self->value.datetime, 0, sizeof(struct tm));
            break;
    }
    return self;
}

TomlValue *toml_value_from_str(TOML_CONST char *str)
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->value.string = toml_string_from_str(str);
    self->type = TOML_STRING;
    return self;
}

TomlValue *toml_value_from_nstr(TOML_CONST char *str, size_t len)
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->value.string = toml_string_from_nstr(str, len);
    self->type = TOML_STRING;
    return self;
}

TomlValue *toml_value_new_table(void)
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->value.table = toml_table_new();
    self->type = TOML_TABLE;
    return self;
}

TomlValue *toml_value_new_array(void)
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->value.array = toml_array_new();
    self->type = TOML_ARRAY;
    return self;
}

#if defined(_MSC_VER) || defined(__APPLE__)
TomlValue *toml_value_new_integer(long long integer)
#else
TomlValue *toml_value_new_integer(long integer)
#endif
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->value.integer = integer;
    self->type = TOML_INTEGER;
    return self;
}

TomlValue *toml_value_new_float(double float_)
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->value.float_ = float_;
    self->type = TOML_FLOAT;
    return self;
}

TomlValue *toml_value_new_datetime(void)
{
    return toml_value_new(TOML_DATETIME);
}

TomlValue *toml_value_new_boolean(int boolean)
{
    TomlValue *self = toml_malloc(sizeof(TomlValue));
    self->value.boolean = boolean;
    self->type = TOML_BOOLEAN;
    return self;
}

void toml_value_free(TomlValue *self)
{
    if (self != NULL) {
        switch (self->type) {
            case TOML_STRING:
                toml_string_free(self->value.string);
                break;
            case TOML_TABLE:
                toml_table_free(self->value.table);
                break;
            case TOML_ARRAY:
                toml_array_free(self->value.array);
                break;
            case TOML_DATETIME:
                memset(&self->value.datetime, 0, sizeof(struct tm));
                break;
            default:
                break;
        }
        free(self);
    }
}

typedef struct _TomlParser {
    TOML_CONST char*    begin;
    TOML_CONST char*    end;
    TOML_CONST char*    ptr;
    int                 lineno;
    int                 colno;
    char*               filename;
} TomlParser;

TomlParser *toml_parser_new(TOML_CONST char *str, size_t len)
{
    TomlParser *self = toml_malloc(sizeof(TomlParser));
    self->begin = str;
    self->end = str + len;
    self->ptr = str;
    self->lineno = 1;
    self->colno = 1;
    self->filename = NULL;
    return self;
}

void toml_parser_free(TomlParser *self)
{
    if (self != NULL) {
        free(self->filename);
        free(self);
    }
}

void toml_move_next(TomlParser *self)
{
    if (self->ptr < self->end) {
        if (*self->ptr == '\n') {
            self->lineno++;
            self->colno = 1;
        } else {
            self->colno++;
        }
        self->ptr++;
    }
}

void toml_next_n(TomlParser *self, int n)
{
    for (int i = 0; i < n; i++) {
        toml_move_next(self);
    }
}

TomlString* toml_parse_bare_key(TomlParser *self)
{
    TOML_CONST char *str = self->ptr;
    size_t len = 0;

    while (self->ptr < self->end) {
        char ch = *self->ptr;

        if (!(isalnum(ch) || ch == '_' || ch == '-'))
            break;

        len++;
        toml_move_next(self);
    }

    return toml_string_from_nstr(str, len);
}

char toml_hex_char_to_int(char ch)
{
    assert(isxdigit(ch));
    if (isdigit(ch)) {
        return ch - '0';
    } else if (islower(ch)) {
        return ch - 'a' + 10;
    } else if (isupper(ch)) {
        return ch - 'A' + 10;
    }
    return 0;
}

int toml_encode_unicode_scalar(TomlString *result, TomlParser *parser, int n)
{
    unsigned int scalar = 0;

    if (parser->ptr + n > parser->end) {
        toml_err_set(TOML_ERR_UNICODE, "%s:%d:%d: invalid unicode scalar",
                     parser->filename, parser->lineno, parser->colno);
        return TOML_ERR_UNICODE;
    }

    for (int i = 0; i < n; i++) {
        char ch = *parser->ptr;
        if (isxdigit(ch)) {
            scalar = scalar * 16 + (unsigned int)toml_hex_char_to_int(ch);
            toml_move_next(parser);
        } else {
            toml_err_set(TOML_ERR_UNICODE, "%s:%d:%d: invalid unicode scalar",
                         parser->filename, parser->lineno, parser->colno);
            return TOML_ERR_UNICODE;
        }
    }

    if ((scalar >= 0xd800 && scalar <= 0xdfff) ||
        (scalar >= 0xfffe && scalar <= 0xffff)) {
        toml_err_set(TOML_ERR_UNICODE, "%s:%d:%d: invalid unicode scalar",
                     parser->filename, parser->lineno, parser->colno);
        return TOML_ERR_UNICODE;
    }

    if (scalar <= 0x7f) {
        toml_string_append_char(result, (char)scalar);
        return 0;
    }

    if (scalar <= 0x7ff) {
        toml_string_append_char(result, (char)(0xc0 | (scalar >> 6)));
        toml_string_append_char(result, (char)(0x80 | (scalar & 0x3f)));
        return 0;
    }

    if (scalar <= 0xffff) {
        toml_string_append_char(result, (char)(0xe0 | (scalar >> 12)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 6) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | (scalar & 0x3f)));
        return 0;
    }

    if (scalar <= 0x1fffff) {
        toml_string_append_char(result, (char)(0xf0 | (scalar >> 18)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 12) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 6) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | (scalar & 0x3f)));
        return 0;
    }

    if (scalar <= 0x3ffffff) {
        toml_string_append_char(result, (char)(0xf8 | (scalar >> 24)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 18) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 12) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 6) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | (scalar & 0x3f)));
        return 0;
    }

    if (scalar <= 0x7fffffff) {
        toml_string_append_char(result, (char)(0xfc | (scalar >> 30)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 24) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 18) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 12) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | ((scalar >> 6) & 0x3f)));
        toml_string_append_char(result, (char)(0x80 | (scalar & 0x3f)));
        return 0;
    }

    toml_err_set(TOML_ERR_UNICODE, "%s:%d:%d: invalid unicode scalar",
                 parser->filename, parser->lineno, parser->colno);

    return TOML_ERR_UNICODE;
}

TomlString* toml_parse_basic_string(TomlParser *self)
{
    TomlString *result = toml_string_new();

    while (self->ptr < self->end && *self->ptr != '\"' && *self->ptr != '\n') {
        char ch1 = *self->ptr;
        if (ch1 == '\\') {
            if (self->ptr >= self->end) break;

            toml_move_next(self);
            char ch2 = *self->ptr;

            if (ch2 == '\"') {
                toml_string_append_char(result, '\"');
                toml_move_next(self);
            } else if (ch2 == 'b') {
                toml_string_append_char(result, '\b');
                toml_move_next(self);
            } else if (ch2 == 't') {
                toml_string_append_char(result, '\t');
                toml_move_next(self);
            } else if (ch2 == 'n') {
                toml_string_append_char(result, '\n');
                toml_move_next(self);
            } else if (ch2 == 'f') {
                toml_string_append_char(result, '\f');
                toml_move_next(self);
            } else if (ch2 == 'r') {
                toml_string_append_char(result, '\r');
                toml_move_next(self);
            } else if (ch2 == '"') {
                toml_string_append_char(result, '\"');
                toml_move_next(self);
            } else if (ch2 == '\\') {
                toml_string_append_char(result, '\\');
                toml_move_next(self);
            } else if (ch2 == 'u') {
                toml_move_next(self);
                if (toml_encode_unicode_scalar(result, self, 4) != 0) {
                    toml_string_free(result);
                    return NULL;
                }
            } else if (ch2 == 'U') {
                toml_move_next(self);
                if (toml_encode_unicode_scalar(result, self, 8) != 0) {
                    toml_string_free(result);
                    return NULL;
                }
            } else {
                toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid escape charactor");
                toml_string_free(result);
            }
        } else {
            toml_string_append_char(result, ch1);
            toml_move_next(self);
        }
    }

    if (self->ptr >= self->end || *self->ptr != '\"' || *self->ptr == '\n') {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated basic string",
                     self->filename, self->lineno, self->colno);
        toml_string_free(result);
    }

    toml_move_next(self);

    return result;
}

TomlString* toml_parse_literal_string(TomlParser *self)
{
    TomlString *result = toml_string_new();

    while (self->ptr < self->end && *self->ptr != '\'' && *self->ptr != '\n') {
        toml_string_append_char(result, *self->ptr);
        toml_move_next(self);
    }

    if (self->ptr >= self->end || *self->ptr != '\'' || *self->ptr == '\n') {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated literal string",
                     self->filename, self->lineno, self->colno);
        toml_string_free(result);
        return NULL;
    }

    toml_move_next(self);

    return result;
}

TomlValue* toml_parse_basic_string_value(TomlParser *self)
{
    TomlValue *value = NULL;

    TomlString *string = toml_parse_basic_string(self);
    if (string == NULL)
        return NULL;

    value = toml_value_new(TOML_STRING);
    value->value.string = string;

    return value;
}

TomlValue *toml_parse_literal_string_value(TomlParser *self)
{
    TomlValue *value = NULL;

    TomlString *string = toml_parse_literal_string(self);
    if (string == NULL)
        return NULL;

    value = toml_value_new(TOML_STRING);
    value->value.string = string;

    return value;
}

TomlValue* toml_parse_multi_line_basic_string(TomlParser *self)
{
    TomlValue *value = NULL;

    TomlString *result = toml_string_new();

    if (*self->ptr == '\n') {
        toml_move_next(self);
    }

    while (self->ptr + 3 <= self->end && strncmp(self->ptr, "\"\"\"", 3) != 0) {
        char ch1 = *self->ptr;

        if (ch1 == '\\') {
            if (self->ptr + 3 > self->end || strncmp(self->ptr, "\"\"\"", 3) == 0)
                break;

            toml_move_next(self);
            char ch2 = *self->ptr;

            if (ch2 == '\"') {
                toml_string_append_char(result, '\"');
                toml_move_next(self);
            } else if (ch2 == 'b') {
                toml_string_append_char(result, '\b');
                toml_move_next(self);
            } else if (ch2 == 't') {
                toml_string_append_char(result, '\t');
                toml_move_next(self);
            } else if (ch2 == 'n') {
                toml_string_append_char(result, '\n');
                toml_move_next(self);
            } else if (ch2 == 'f') {
                toml_string_append_char(result, '\f');
                toml_move_next(self);
            } else if (ch2 == 'r') {
                toml_string_append_char(result, '\r');
                toml_move_next(self);
            } else if (ch2 == '"') {
                toml_string_append_char(result, '\"');
                toml_move_next(self);
            } else if (ch2 == '\\') {
                toml_string_append_char(result, '\\');
                toml_move_next(self);
            } else if (ch2 == 'u') {
                toml_move_next(self);
                if (toml_encode_unicode_scalar(result, self, 4) != 0) {
                    toml_string_free(result);
                    return NULL;
                }
            } else if (ch2 == 'U') {
                toml_move_next(self);
                if (toml_encode_unicode_scalar(result, self, 8) != 0) {
                    toml_string_free(result);
                    return NULL;
                }
            } else if (ch2 == '\n') {
                do {
                    toml_move_next(self);
                } while (self->ptr + 3 <= self->end && isspace(*self->ptr));
            } else {
                toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid escape charactor",
                             self->filename, self->lineno, self->colno);
                toml_string_free(result);
                return NULL;
            }
        } else {
            toml_string_append_char(result, ch1);
            toml_move_next(self);
        }
    }

    if (self->ptr + 3 > self->end || strncmp(self->ptr, "\"\"\"", 3) != 0) {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated multi-line basic string",
                     self->filename, self->lineno, self->colno);
        toml_string_free(result);
        return NULL;
    }

    toml_next_n(self, 3);

    value = toml_value_new(TOML_STRING);
    value->value.string = result;

    return value;
}

TomlValue* toml_parse_multi_line_literal_string(TomlParser *self)
{
    TomlValue *value = NULL;

    TomlString *result = toml_string_new();

    if (*self->ptr == '\n') {
        toml_move_next(self);
    }

    while (self->ptr + 3 <= self->end && strncmp(self->ptr, "\'\'\'", 3) != 0) {
        toml_string_append_char(result, *self->ptr);
        toml_move_next(self);
    }

    if (self->ptr + 3 > self->end || strncmp(self->ptr, "\'\'\'", 3) != 0) {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated multi-line literal string",
                     self->filename, self->lineno, self->colno);
        toml_string_free(result);
        return NULL;
    }

    toml_next_n(self, 3);

    value = toml_value_new(TOML_STRING);
    value->value.string = result;

    return value;
}

TomlValue* toml_parse_datetime(TOML_CONST char *str, size_t len)
{
    (void)str;
    (void)len;
    return toml_value_new(TOML_DATETIME);
}

TomlValue* toml_parse_int_or_float_or_time(TomlParser *self)
{
    TomlString *str = NULL;
    TomlValue *result = NULL;

    char type = 'i';
    int base = 10;

    str = toml_string_new();

    // Determine nan and inf type as float, as we cannot determine by dot.
    // But do not strip it because we will append it to the string later
    if (self->ptr + 3 <= self->end &&
        (strncmp(self->ptr, "nan", 3) == 0 || strncmp(self->ptr, "inf", 3) == 0)) {
        type = 'f';
    }

    if (self->ptr + 4 <= self->end &&
        (strncmp(self->ptr, "+nan", 4) == 0 ||
         strncmp(self->ptr, "-nan", 4) == 0 ||
         strncmp(self->ptr, "+inf", 4) == 0 ||
         strncmp(self->ptr, "-inf", 4) == 0)) {
        type = 'f';
    }

    // If there is a base prefix, set the base and strip the prefix,
    // because strtoll() do not recognize 0o and 0b
    if (self->ptr + 2 <= self->end) {
        if (strncmp(self->ptr, "0x", 2) == 0) {
            base = 16;
            toml_next_n(self, 2);
        } else if (strncmp(self->ptr, "0o", 2) == 0) {
            base = 8;
            toml_next_n(self, 2);
        } else if (strncmp(self->ptr, "0b", 2) == 0) {
            base = 2;
            toml_next_n(self, 2);
        }
    }

    char last_char = 0;
    int has_exp = TOML_FALSE;
    while (self->ptr < self->end) {
        if (*self->ptr == '+' || *self->ptr == '-') {
            if (last_char == 0 || ((last_char == 'e' || last_char == 'E') && !has_exp)) {
                if (last_char != 0) {
                    type = 'f';
                    has_exp = TOML_TRUE;
                }
                toml_string_append_char(str, *self->ptr);
            } else {
                break;
            }
        } else if (isalnum(*self->ptr)) {
            if ((*self->ptr == 'e' || *self->ptr == 'E') && base == 10) {
                type = 'f';
            }

            toml_string_append_char(str, *self->ptr);
        } else if (*self->ptr == '.') {
            if (type == 'i') {
                type = 'f';
                toml_string_append_char(str, *self->ptr);
            } else {
                toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid float",
                             self->filename, self->lineno, self->colno);
                toml_string_free(str);
                return NULL;
            }
        } else if (*self->ptr == '_') {
            if (type == 't') {
                toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid datetime",
                             self->filename, self->lineno, self->colno);
                toml_string_free(str);
                return NULL;
            }

            if (!isalnum(last_char)) {
                toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid integer or float",
                             self->filename, self->lineno, self->colno);
                toml_string_free(str);
                return NULL;
            }
        } else if (*self->ptr == '-') {
            type = 't';
            toml_string_append_char(str, *self->ptr);
        } else {
            break;
        }

        last_char = *self->ptr;
        toml_move_next(self);
    }

    if (last_char == '_') {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid integer or float or datetime",
                     self->filename, self->lineno, self->colno);
        toml_string_free(str);
        return NULL;
    }

    if (type == 'i') {
        char *end = NULL;
        long long n = strtoll(str->str, &end, base);
        if (end < str->str + str->len) {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid integer",
                         self->filename, self->lineno, self->colno);
            toml_string_free(str);
            return NULL;
        }
        result = toml_value_new_integer(n);
    } else if (type == 'f') {
        char *end = NULL;
        double n = strtod(str->str, &end);
        if (end < str->str + str->len) {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: invalid float");
            goto cleanup;
        }
        result = toml_value_new_float(n);
    } else if (type == 't') {
        result = toml_parse_datetime(str->str, str->len);
    }

cleanup:
    toml_string_free(str);
    return result;
}

TomlValue* toml_parse_bool(TomlParser *self)
{
    if (self->ptr + 4 <= self->end && strncmp(self->ptr, "true", 4) == 0 &&
        (self->ptr + 4 == self->end || isspace(*(self->ptr + 4)) || *(self->ptr + 4) == ',' || *(self->ptr + 4) == ']' || *(self->ptr + 4) == '}')) {
        toml_next_n(self, 4);
        return toml_value_new_boolean(TOML_TRUE);
    }

    if (self->ptr + 5 <= self->end && strncmp(self->ptr, "false", 5) == 0 &&
        (self->ptr + 5 == self->end || isspace(*(self->ptr + 5)) || *(self->ptr + 4) == ',' || *(self->ptr + 4) == ']' || *(self->ptr + 4) == '}')) {
        toml_next_n(self, 5);
        return toml_value_new_boolean(TOML_FALSE);
    }

    return NULL;
}

TomlValue* toml_parse_array(TomlParser *self);
TomlValue* toml_parse_inline_table(TomlParser *self);

TomlValue* toml_parse_value(TomlParser *self, int allow_comma)
{
    TomlValue *value = NULL;

    char ch = *self->ptr;

    if (strncmp(self->ptr, "\"\"\"", 3) == 0) {
        toml_next_n(self, 3);
        value = toml_parse_multi_line_basic_string(self);
    } else if (strncmp(self->ptr, "\'\'\'", 3) == 0) {
        toml_next_n(self, 3);
        value = toml_parse_multi_line_literal_string(self);
    } else if (ch == '\"') {
        toml_move_next(self);
        value = toml_parse_basic_string_value(self);
    } else if (ch == '\'') {
        toml_move_next(self);
        value = toml_parse_literal_string_value(self);
    } else if (isdigit(ch) || ch == '+' || ch == '-' || ch == '.' || ch == 'n' || ch == 'i') {
        value = toml_parse_int_or_float_or_time(self);
    } else if (ch == 't' || ch == 'f') {
        value = toml_parse_bool(self, allow_comma);
    } else if (ch == '[') {
        toml_move_next(self);
        value = toml_parse_array(self);
    } else if (ch == '{') {
        toml_move_next(self);
        value = toml_parse_inline_table(self);
    } else {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                     self->filename, self->lineno, self->colno);
    }

    return value;
}

TomlErrCode toml_parse_key_value(TomlParser *self, TomlTable *table, int allow_comma)
{
    TomlString *key = NULL;
    TomlValue *value = NULL;

    while (self->ptr < self->end) {
        char ch;

        ch = *self->ptr;
        while (self->ptr < self->end && isspace(ch)) {
            toml_move_next(self);
            ch = *self->ptr;
        }

        if (self->ptr == self->end) break;

        if (isalnum(ch) || ch == '_') {
            key = toml_parse_bare_key(self);
            if (key == NULL)
                return toml_err()->code;
        } else if (ch == '\"') {
            toml_move_next(self);
            key = toml_parse_basic_string(self);
            if (key == NULL)
                return toml_err()->code;
        } else if (ch == '\'') {
            toml_move_next(self);
            key = toml_parse_literal_string(self);
            if (key == NULL)
                return toml_err()->code;
        } else if (ch == '[') {
            break;
        } else if (ch == '#') {
            do {
                toml_move_next(self);
                ch = *self->ptr;
            } while (self->ptr < self->end && ch != '\n');
            toml_move_next(self);
            continue;
        } else {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                         self->filename, self->lineno, self->colno);
            return TOML_ERR_SYNTAX;
        }

        ch = *self->ptr;
        while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
            toml_move_next(self);
            ch = *self->ptr;
        }

        if (self->ptr == self->end) {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
            return TOML_ERR_SYNTAX;
        }

        if (ch != '=') {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                         self->filename, self->lineno, self->colno);
            return TOML_ERR_SYNTAX;
        }

        toml_move_next(self);

        ch = *self->ptr;
        while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
            toml_move_next(self);
            ch = *self->ptr;
        }

        if (self->ptr == self->end) {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
            return TOML_ERR_SYNTAX;
        }

        value = toml_parse_value(self, allow_comma);
        if (value == NULL)
            return toml_err()->code;

        toml_table_set_by_string(table, key, value);

        key = NULL;
        value = NULL;

        while (self->ptr < self->end && (*self->ptr == ' ' || *self->ptr == '\t')) {
            toml_move_next(self);
        }

        if (self->ptr == self->end)
            break;

        if (*self->ptr == '#') {
            do {
                toml_move_next(self);
            } while (self->ptr < self->end && *self->ptr != '\n');
        }

        if (*self->ptr == '\r') {
            toml_move_next(self);
        }

        if (*self->ptr == '\n') {
            toml_move_next(self);
        } else {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: new line expected",
                         self->filename, self->lineno, self->colno);
            toml_value_free(value);
            toml_string_free(key);
            return TOML_ERR_SYNTAX;
        }
    }

    return TOML_OK;
}

TomlValue* toml_parse_array(TomlParser *self)
{
    TomlValue *array = NULL;
    TomlValue *value = NULL;

    array = toml_value_new_array();

    while (self->ptr < self->end) {
        if (isspace(*self->ptr)) {
            while (self->ptr < self->end && isspace(*self->ptr)) {
                toml_move_next(self);
            }
        } else if (*self->ptr == '#') {
            do {
                toml_move_next(self);
            } while (self->ptr < self->end && *self->ptr != '\n');
            toml_move_next(self);
        } else if (*self->ptr == '\n') {
            toml_move_next(self);
        } else if (*self->ptr == ']') {
            toml_move_next(self);
            break;
        } else {
            value = toml_parse_value(self, 1);
            if (value == NULL) {
                goto error;
            }

            toml_array_append(array->value.array, value);

            value = NULL;

            while (self->ptr < self->end) {
                if (isspace(*self->ptr)) {
                    do {
                        toml_move_next(self);
                    } while (self->ptr < self->end && isspace(*self->ptr));
                } else if (*self->ptr == '#') {
                    do {
                        toml_move_next(self);
                    } while (self->ptr < self->end && *self->ptr != '\n');
                } else {
                    break;
                }
            }

            if (self->ptr < self->end && *self->ptr == ',') {
                toml_move_next(self);
            }
        }
    }

    goto end;

error:
    toml_value_free(value);
    toml_value_free(array);
    array = NULL;

end:
    return array;
}

TomlValue* toml_parse_inline_table(TomlParser *self)
{
    TomlValue *table = NULL;

    table = toml_value_new_table();

    while (self->ptr < self->end) {
        TomlString *key = NULL;
        TomlValue *value = NULL;
        char ch;

        ch = *self->ptr;
        while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
            toml_move_next(self);
            ch = *self->ptr;
        }

        if (isalnum(ch) || ch == '_') {
            key = toml_parse_bare_key(self);
            if (key == NULL)
                goto error;
        } else if (ch == '\"') {
            toml_move_next(self);
            key = toml_parse_basic_string(self);
            if (key == NULL)
                goto error;
        } else if (ch == '\'') {
            toml_move_next(self);
            key = toml_parse_literal_string(self);
            if (key == NULL)
                goto error;
        } else if (ch == '}') {
            break;
        } else {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                         self->filename, self->lineno, self->colno);
            goto error;
        }

        ch = *self->ptr;
        while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
            toml_move_next(self);
            ch = *self->ptr;
        }

        if (self->ptr == self->end) {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
            goto error;
        }

        if (ch != '=') {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                         self->filename, self->lineno, self->colno);
            goto error;
        }

        toml_move_next(self);

        ch = *self->ptr;
        while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
            toml_move_next(self);
            ch = *self->ptr;
        }

        if (self->ptr == self->end) {
            toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
            goto error;
        }

        value = toml_parse_value(self, 1);
        if (value == NULL)
            goto error;

        toml_table_set_by_string(table->value.table, key, value);

        while (self->ptr < self->end && (*self->ptr == ' ' || *self->ptr == '\t')) {
            toml_move_next(self);
        }

        if (*self->ptr == ',') {
            toml_move_next(self);
        } else if (*self->ptr == '}') {
            toml_move_next(self);
            break;
        }
    }

    goto end;

error:
    toml_value_free(table);
    table = NULL;

end:
    return table;
}

TomlTable* toml_walk_table_path(TomlParser *parser, TomlTable *table,
                                TomlArray *key_path, int is_array,
                                int create_if_not_exist)
{
    TomlTable *real_table = table;
    TomlValue *new_table = NULL;
    TomlValue *array = NULL;
    TomlString *part_copy = NULL;

    if (is_array) {
        size_t i = 0;
        for (; i < key_path->len - 1; i++) {
            TomlString *part = key_path->elements[i]->value.string;
            TomlValue *t = toml_table_get_by_string(real_table, part);
            if (t == NULL) {
                if (create_if_not_exist) {
                    new_table = toml_value_new_table();
                    part_copy = toml_string_clone(part);
                    toml_table_set_by_string(real_table, part_copy, new_table);
                    real_table = new_table->value.table;
                    part_copy = NULL;
                    new_table = NULL;
                } else {
                    real_table = NULL;
                    break;
                }
            } else {
                real_table = t->value.table;
            }
        }

        TomlString *part = key_path->elements[i]->value.string;
        TomlValue *t = toml_table_get_by_string(real_table, part);
        if (t == NULL) {
            if (create_if_not_exist) {
                array = toml_value_new_array();
                new_table = toml_value_new_table();
                toml_array_append(array->value.array, new_table);
                part_copy = toml_string_clone(part);
                toml_table_set_by_string(real_table, part_copy, array);
                real_table = new_table->value.table;
                part_copy = NULL;
                array = NULL;
                new_table = NULL;
            } else {
                real_table = NULL;
            }
        } else {
            if (t->type != TOML_ARRAY) {
                toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: this key was not an array",
                             parser->filename, parser->lineno, parser->colno);
                goto error;
            }

            TomlValue *new_table = toml_value_new_table();
            toml_array_append(t->value.array, new_table);
            real_table = new_table->value.table;
        }
    } else {
        for (size_t i = 0; i < key_path->len; i++) {
            TomlString *part = key_path->elements[i]->value.string;
            TomlValue *t = toml_table_get_by_string(real_table, part);
            if (t == NULL) {
                if (create_if_not_exist) {
                    new_table = toml_value_new_table();
                    part_copy = toml_string_clone(part);
                    toml_table_set_by_string(real_table, part_copy, new_table);
                    real_table = new_table->value.table;
                    part_copy = NULL;
                    new_table = NULL;
                } else {
                    real_table = NULL;
                    break;
                }
            } else {
                if (t->type == TOML_ARRAY) {
                    real_table = t->value.array->elements[t->value.array->len - 1]->value.table;
                } else if (t->type == TOML_TABLE) {
                    real_table = t->value.table;
                }
            }
        }
    }

    goto end;

error:
    toml_string_free(part_copy);
    toml_value_free(new_table);
    toml_value_free(array);
    real_table = NULL;

end:
    return real_table;
}

TomlErrCode toml_parse_table(TomlParser *self, TomlTable *table)
{
    TomlArray *key_path = NULL;
    int is_array = TOML_FALSE;
    TomlTable *real_table = table;
    TomlErrCode rc = 0;

    key_path = toml_array_new();

    if (self->ptr < self->end && *self->ptr == '[') {
        is_array = TOML_TRUE;
        toml_move_next(self);
    }

    while (1) {
        if (*self->ptr == ' ' || *self->ptr == '\t') {
            do {
                toml_move_next(self);
            } while (*self->ptr < *self->end && (*self->ptr == ' ' || *self->ptr == '\t'));
        } else if (*self->ptr == ']') {
            if (is_array) {
                if (self->ptr + 2 <= self->end && strncmp(self->ptr, "]]", 2) == 0) {
                    toml_next_n(self, 2);
                    break;
                }
            } else {
                toml_move_next(self);
                break;
            }
        } else {
            TomlString *key_part = NULL;
            TomlValue *key_part_value = NULL;

            if (isalnum(*self->ptr) || *self->ptr == '_') {
                key_part = toml_parse_bare_key(self);
                if (key_part == NULL)
                    goto cleanup;
            } else if (*self->ptr == '\"') {
                toml_move_next(self);
                key_part = toml_parse_basic_string(self);
                if (key_part == NULL)
                    goto cleanup;
            } else if (*self->ptr == '\'') {
                toml_move_next(self);
                key_part = toml_parse_literal_string(self);
                if (key_part == NULL)
                    goto cleanup;
            } else {
                toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                             self->filename, self->lineno, self->colno);
            }

            key_part_value = toml_value_new(TOML_STRING);
            key_part_value->value.string = key_part;

            toml_array_append(key_path, key_part_value);

            while (self->ptr < self->end &&
                   (*self->ptr == ' ' || *self->ptr == '\t')) {
                toml_move_next(self);
            }

            if (self->ptr < self->end && *self->ptr == '.') {
                toml_move_next(self);
            }
        }
    }

    if (key_path->len == 0) {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: empty table name",
                     self->filename, self->lineno, self->colno);
        goto cleanup;
    }

    while (self->ptr < self->end &&
           (*self->ptr == ' ' || *self->ptr == '\t' || *self->ptr == '\r')) {
        toml_move_next(self);
    }

    if (self->ptr < self->end && *self->ptr != '\n') {
        toml_err_set(TOML_ERR_SYNTAX, "%s:%d:%d: new line expected",
                     self->filename, self->lineno, self->colno);
        goto cleanup;
    }

    real_table = toml_walk_table_path(self, table, key_path, is_array, TOML_TRUE);
    if (real_table == NULL)
        goto error;

    toml_parse_key_value(self, real_table, 0);

    goto cleanup;

error:
    rc = toml_err()->code;
cleanup:
    toml_array_free(key_path);
    return rc;
}

TomlTable* toml_parse(TomlParser *self)
{
    TomlTable *table = NULL;

    table = toml_table_new();

    while (self->ptr < self->end) {
        char ch = *self->ptr;

        while (self->ptr < self->end && isspace(ch)) {
            toml_move_next(self);
            ch = *self->ptr;
        }

        if (ch == '#') {
            do {
                toml_move_next(self);
                ch = *self->ptr;
            } while (self->ptr < self->end && ch != '\n');
            toml_move_next(self);
        } else if (ch == '[') {
            toml_move_next(self);
            if (toml_parse_table(self, table) != 0)
                return NULL;
        } else if (isalnum(ch) || ch == '_' || ch == '-') {
            if (toml_parse_key_value(self, table, 0) != 0)
                return NULL;
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            do {
                toml_move_next(self);
                ch = *self->ptr;
            } while (ch == ' ' || ch == '\t' || ch == '\r');
        } else if (ch == '\n') {
            toml_move_next(self);
        }
    }

    return table;
}

TomlTable* toml_load_nstr_filename(TOML_CONST char *str, size_t len,
                                   TOML_CONST char *filename)
{
    TomlParser *parser = NULL;
    TomlTable *table = NULL;

    parser = toml_parser_new(str, len);
    parser->filename = toml_strdup(filename);

    table = toml_parse(parser);

    toml_parser_free(parser);
    return table;
}

TomlTable* toml_load_nstr(TOML_CONST char *str, size_t len)
{
    return toml_load_nstr_filename(str, len, "<string>");
}

TomlTable* toml_load_str(TOML_CONST char *str)
{
    return toml_load_nstr(str, sizeof(str));
}

TomlTable* toml_load_file_filename(FILE *file, TOML_CONST char *filename)
{
    TomlTable *table = NULL;
    TomlString *str = NULL;

    str = toml_string_new();

    toml_string_expand_if_necessary(str, 4095);

    size_t count;
    size_t bytes_to_read;
    do {
        bytes_to_read = str->_capacity - str->len - 1;

        count = fread(str->str, 1, bytes_to_read, file);
        if (ferror(file)) {
            toml_err_set(TOML_ERR_OS, "Error when reading %s [errno %d: %s]", filename, errno, strerror(errno));
            goto error;
        }

        if (str->len + 1 >= str->_capacity) {
            toml_string_expand_if_necessary(str, str->_capacity * 2);
        }

        str->len += count;
    } while (count == bytes_to_read);

    str->str[str->len] = 0;

    table = toml_load_nstr_filename(str->str, str->len, filename);

    goto cleanup;

error:
    toml_table_free(table);
    table = NULL;

cleanup:
    toml_string_free(str);
    return table;
}

TomlTable* toml_load_file(FILE *file)
{
    return toml_load_file_filename(file, "<stream>");
}

TomlTable* toml_load_filename(TOML_CONST char *filename)
{
    TomlTable *table = NULL;
    FILE *f = NULL;

    f = fopen(filename, "r");
    if (f == NULL) {
        toml_err_set(TOML_ERR_OS, "Cannot open file %s [errno %d: %s]", filename, errno, strerror(errno));
        goto error;
    }

    table = toml_load_file_filename(f, filename);

    goto cleanup;

error:
    toml_table_free(table);
    table = NULL;

cleanup:
    if (f != NULL)
        fclose(f);
    return table;
}

// vim: sw=4 ts=8 sts=4 et
