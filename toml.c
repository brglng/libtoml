#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "toml.h"

char *toml_strdup(const char *str)
{
  size_t len = strlen(str) + 1;
  void *new = malloc(len);
  if (new == NULL)
    return NULL;

  return memcpy(new, str, len);
}

char *toml_strndup(const char *str, size_t n)
{
  char *result = malloc(n + 1);
  if (result == NULL)
    return NULL;

  result[n] = 0;
  return memcpy(result, str, n);
}

int toml_vasprintf(char **str, const char *format, va_list args)
{
  int size = 0;

  va_list args_copy;
  va_copy(args_copy, args);
  size = vsnprintf(NULL, size, format, args_copy);
  va_end(args_copy);

  if (size < 0) {
    return size;
  }

  *str = malloc(size + 1);
  if (*str == NULL)
    return -1;

  return vsprintf(*str, format, args);
}

int toml_asprintf(char **str, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int size = toml_vasprintf(str, format, args);
  va_end(args);
  return size;
}

void toml_err_init(TomlErr *err)
{
  err->code = TOML_OK;
  err->message = NULL;
  err->_is_literal = false;
}

void toml_set_err_v(TomlErr *err, int code, const char *format, va_list args)
{
  if (err != NULL) {
    assert(err->code == TOML_OK);
    err->code = code;
    toml_vasprintf(&err->message, format, args);
    err->_is_literal = false;
  }
}

void toml_set_err(TomlErr *err, int code, const char *format, ...)
{
  if (err != NULL) {
    assert(err->code == TOML_OK);
    va_list args;
    va_start(args, format);
    toml_set_err_v(err, code, format, args);
    va_end(args);
  }
}

void toml_set_err_literal(TomlErr *err, int code, const char *message)
{
  if (err != NULL) {
    assert(err->code == TOML_OK);
    err->code = code;
    err->message = (char *)message;
    err->_is_literal = true;
  }
}

void toml_clear_err(TomlErr *err)
{
  if (err) {
    if (!err->_is_literal) {
      free(err->message);
    }
    toml_err_init(err);
  }
}

void toml_err_move(TomlErr *to, TomlErr *from)
{
  if (to == NULL) {
    if (!from->_is_literal) {
      free(from->message);
    }
  } else {
    assert(to->code == TOML_OK);
    *to = *from;
    toml_err_init(from);
  }
}

static inline size_t toml_roundup_pow_of_two_size_t(size_t x)
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

TomlString *toml_string_new(TomlErr *error)
{
  TomlString *self = malloc(sizeof(TomlString));
  if (self == NULL) {
    toml_set_err_literal(error, TOML_ERR_MEM, "out of memory");
    return NULL;
  }

  self->str = NULL;
  self->len = 0;
  self->_capacity = 0;

  return self;
}

TomlString *toml_string_new_string(const char *str, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlString *self = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  toml_string_append_string(self, str, &err);

cleanup:
  if (err.code != TOML_OK) {
    free(self);
    self = NULL;
  }
  toml_err_move(error, &err);
  return self;
}

TomlString *toml_string_new_nstring(const char *str, size_t len, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlString *self = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  toml_string_append_nstring(self, str, len, &err);

cleanup:
  if (err.code != TOML_OK) {
    free(self);
    self = NULL;
  }
  toml_err_move(error, &err);
  return self;
}

void toml_string_check_expand(TomlString *self, size_t len_to_add, TomlErr *error)
{
  if (self->len + len_to_add + 1 > self->_capacity) {
    size_t new_capacity = toml_roundup_pow_of_two_size_t(self->len + len_to_add + 1);
    new_capacity = new_capacity >= 8 ? new_capacity : 8;
    void *p = realloc(self->str, new_capacity);
    if (p == NULL) {
      toml_set_err_literal(error, TOML_ERR_MEM, "out of memory");
      return;
    }
    self->str = p;
    self->_capacity = new_capacity;
  }
}

void toml_string_append_char(TomlString *self, char ch, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  toml_string_check_expand(self, 1, &err);
  if (err.code != TOML_OK) goto cleanup;

  self->str[self->len] = ch;
  self->str[self->len + 1] = 0;
  self->len++;

cleanup:
  toml_err_move(error, &err);
}

void toml_string_append_string(TomlString *self, const char *str, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  size_t len = strlen(str);

  toml_string_check_expand(self, len, &err);
  if (err.code != TOML_OK) goto cleanup;

  memcpy(&self->str[self->len], str, len + 1);
  self->len += len;

cleanup:
  toml_err_move(error, &err);
}

void toml_string_append_nstring(TomlString *self, const char *str, size_t len, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  toml_string_check_expand(self, len, &err);
  if (err.code != TOML_OK) goto cleanup;

  memcpy(&self->str[self->len], str, len);
  self->len += len;
  self->str[self->len] = 0;

cleanup:
  toml_err_move(error, &err);
}

void toml_string_free(TomlString *self)
{
  if (self != NULL) {
    free(self->str);
    free(self);
  }
}

bool toml_string_equals(const TomlString *self, const TomlString *other)
{
  if (self == other) {
    return true;
  }

  if (self->len != other->len) {
    return false;
  }

  if (self->str == other->str) {
    return true;
  }

  for (size_t i = 0; i < self->len; i++) {
    if (self->str[i] != other->str[i]) {
      return false;
    }
  }

  return true;
}

struct _TomlTable {
  TomlKeyValue  *keyvals;
  size_t        len;
  size_t        capacity;
};

TomlTable *toml_table_new(TomlErr *err)
{
  TomlTable *self = malloc(sizeof(TomlTable));
  if (self == NULL) {
    toml_set_err_literal(err, TOML_ERR_MEM, "out of memory");
  } else {
    self->keyvals = NULL;
    self->len = 0;
    self->capacity = 0;
  }
  return self;
}

void toml_table_free(TomlTable *self)
{
  if (self != NULL) {
    free(self->keyvals);
    free(self);
  }
}

void toml_table_check_expand(TomlTable *self, TomlErr *err)
{
  if (self->len + 1 > self->capacity) {
    size_t new_capacity = self->capacity > 0 ? self->capacity * 2 : 8;
    void *p = realloc(self->keyvals, sizeof(TomlKeyValue) * new_capacity);
    if (p == NULL) {
      toml_set_err_literal(err, TOML_ERR_MEM, "out of memory");
      return;
    }
    self->keyvals = p;
    self->capacity = new_capacity;
  }
}

void toml_table_set(TomlTable *self, TomlString *key, TomlValue *value,
                    TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue **slot = NULL;
  for (size_t i = 0; i < self->len; i++) {
    if (toml_string_equals(self->keyvals[i].key, key)) {
      slot = &self->keyvals[i].value;
    }
  }

  if (slot == NULL) {
    toml_table_check_expand(self, &err);
    if (err.code != TOML_OK) {
      toml_err_move(error, &err);
      return;
    }

    self->keyvals[self->len].key = key;
    self->keyvals[self->len].value = value;
    self->len++;
  } else {
    *slot = value;
  }
}

TomlValue *toml_table_get(const TomlTable *self, const TomlString *key)
{
  TomlValue *value = NULL;
  for (size_t i = 0; i < self->len; i++) {
    if (toml_string_equals(self->keyvals[i].key, key)) {
      value = self->keyvals[i].value;
    }
  }
  return value;
}

struct _TomlTableIter {
  TomlTable *table;
  TomlKeyValue *keyval;
};

TomlTableIter *toml_table_iter_new(TomlTable *table, TomlErr *error)
{
  TomlTableIter *self = malloc(sizeof(TomlTableIter));
  if (self == NULL) {
    toml_set_err_literal(error, TOML_ERR_MEM, "out of memory");
    return NULL;
  }
  self->table = table;
  self->keyval = table->keyvals;
  return self;
}

void toml_table_iter_free(TomlTableIter *self)
{
  free(self);
}

TomlKeyValue *toml_table_iter_get(TomlTableIter *self)
{
  return self->keyval;
}

bool toml_table_iter_has_next(TomlTableIter *self)
{
  return self->keyval != NULL;
}

void toml_table_iter_next(TomlTableIter *self)
{
  if (self->keyval < self->table->keyvals + self->table->len) {
    self->keyval++;
  }

  if (self->keyval >= self->table->keyvals + self->table->len) {
    self->keyval = NULL;
  }
}

TomlArray *toml_array_new(TomlErr *error)
{
  TomlArray *self = malloc(sizeof(TomlArray));
  if (self == NULL) {
    toml_set_err_literal(error, TOML_ERR_MEM, "out of memory");
    return NULL;
  }

  self->elements = NULL;
  self->len = 0;
  self->_capacity = 0;
  return self;
}

void toml_array_free(TomlArray *self)
{
  if (self != NULL) {
    free(self->elements);
    free(self);
  }
}

void toml_array_check_expand(TomlArray *self, TomlErr *error)
{
  if (self->len + 1 > self->_capacity) {
    size_t new_capacity = self->_capacity > 0 ? self->_capacity * 2 : 8;
    void *p = realloc(self->elements, sizeof(TomlValue *) * new_capacity);
    if (p == NULL) {
      toml_set_err_literal(error, TOML_ERR_MEM, "out of memory");
      return;
    }
    self->elements = p;
    self->_capacity = new_capacity;
  }
}

void toml_array_append(TomlArray *self, TomlValue *value, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  toml_array_check_expand(self, &err);
  if (err.code != TOML_OK) goto cleanup;

  self->elements[self->len++] = value;

cleanup:
  toml_err_move(error, &err);
}

TomlValue *toml_value_new(TomlType type, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->type = type;
  switch (type) {
  case TOML_TABLE:      self->value.table = NULL;       break;
  case TOML_ARRAY:      self->value.array = NULL;       break;
  case TOML_STRING:     self->value.string = NULL;      break;
  case TOML_INTEGER:    self->value.integer = 0;        break;
  case TOML_FLOAT:      self->value.float_ = 0.0;       break;
  case TOML_DATETIME:                                   break;
  case TOML_BOOLEAN:    self->value.boolean = false;    break;
  }

cleanup:
  toml_err_move(error, &err);
  return self;
}

TomlValue *toml_value_new_string(const char *str, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->value.string = toml_string_new_string(str, &err);
  if (err.code != TOML_OK) {
    free(self);
    self = NULL;
    goto cleanup;
  }

  self->type = TOML_STRING;

cleanup:
  toml_err_move(error, &err);
  return self;
}

TomlValue *toml_value_new_nstring(const char *str, size_t len, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->value.string = toml_string_new_nstring(str, len, &err);
  if (err.code != TOML_OK) {
    free(self);
    self = NULL;
    goto cleanup;
  }

  self->type = TOML_STRING;

cleanup:
  toml_err_move(error, &err);
  return self;
}

TomlValue *toml_value_new_table(TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->value.table = toml_table_new(&err);
  if (err.code != TOML_OK) {
    free(self);
    self = NULL;
    goto cleanup;
  }

  self->type = TOML_TABLE;

cleanup:
  toml_err_move(error, &err);
  return self;
}

TomlValue *toml_value_new_array(TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->value.array = toml_array_new(&err);
  if (err.code != TOML_OK) {
    free(self);
    self = NULL;
    goto cleanup;
  }

  self->type = TOML_ARRAY;

cleanup:
  toml_err_move(error, &err);
  return self;
}

TomlValue *toml_value_new_integer(int64_t integer, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->value.integer = integer;
  self->type = TOML_INTEGER;

cleanup:
  toml_err_move(error, &err);
  return self;
}

TomlValue *toml_value_new_float(double float_, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->value.float_ = float_;
  self->type = TOML_FLOAT;

cleanup:
  toml_err_move(error, &err);
  return self;
}

TomlValue *toml_value_new_boolean(bool boolean, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *self = malloc(sizeof(TomlValue));
  if (self == NULL) {
    toml_set_err_literal(&err, TOML_ERR_MEM, "out of memory");
    goto cleanup;
  }

  self->value.boolean = boolean;
  self->type = TOML_BOOLEAN;

cleanup:
  toml_err_move(error, &err);
  return self;
}

void toml_value_free(TomlValue *self)
{
  if (self != NULL) {
    switch (self->type) {
    case TOML_STRING:   toml_string_free(self->value.string);   break;
    case TOML_TABLE:    toml_table_free(self->value.table);     break;
    case TOML_ARRAY:    toml_array_free(self->value.array);     break;
    default:                                                    break;
    }
    free(self);
  }
}

typedef struct {
  const char *begin;
  const char *end;
  const char *ptr;
  int lineno;
  int colno;
  char *filename;
} TomlParser;

TomlParser *toml_parser_new(const char *str, size_t len, TomlErr *error)
{
  TomlParser *self = malloc(sizeof(TomlParser));
  if (self == NULL) {
    toml_set_err_literal(error, TOML_OK, "out of memory");
    return NULL;
  }

  self->begin = str;
  self->end = str + len;
  self->ptr = str;
  self->lineno = 1;
  self->colno = 1;
  self->filename = "<string>";

  return self;
}

void toml_parser_free(TomlParser *self)
{
  if (self != NULL) {
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

TomlString *toml_parse_bare_key(TomlParser *self, TomlErr *error)
{
  const char *str = self->ptr;
  size_t len = 0;

  while (self->ptr < self->end) {
    char ch = *self->ptr;

    if (!(isalnum(ch) || ch == '_' || ch == '-'))
      break;

    len++;
    toml_move_next(self);
  }

  return toml_string_new_nstring(str, len, error);
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
  assert(false);
}

void toml_encode_unicode_scalar16(TomlString *result, TomlParser *parser, TomlErr *error)
{
  uint16_t scalar = 0;

  if (parser->ptr + 4 >= parser->end) {
    toml_set_err(error, TOML_ERR_SYNTAX, "%s:%d:%d: invalid unicode scalar",
                 parser->filename, parser->lineno, parser->colno);
    return;
  }

  for (int i = 0; i < 4; i++) {
    char ch = *parser->ptr;
    if (isxdigit(ch)) {
      scalar = scalar * 16 + toml_hex_char_to_int(ch);
      toml_move_next(parser);
    } else {
      toml_set_err(error, TOML_ERR_SYNTAX, "%s:%d:%d: invalid unicode scalar",
                   parser->filename, parser->lineno, parser->colno);
      break;
    }
  }

  (void)result;
}

void toml_encode_unicode_scalar32(TomlString *result, TomlParser *parser, TomlErr *error)
{
  uint32_t scalar = 0;

  if (parser->ptr + 8 >= parser->end) {
    toml_set_err(error, TOML_ERR_SYNTAX, "%s:%d:%d: invalid unicode scalar",
                 parser->filename, parser->lineno, parser->colno);
    return;
  }

  for (int i = 0; i < 8; i++) {
    char ch = *parser->ptr;
    if (isxdigit(ch)) {
      scalar = scalar * 16 + toml_hex_char_to_int(ch);
      toml_move_next(parser);
    } else {
      toml_set_err(error, TOML_ERR_SYNTAX, "%s:%d:%d: invalid unicode scalar",
                   parser->filename, parser->lineno, parser->colno);
      break;
    }
  }

  (void)result;
}

TomlString *toml_parse_basic_string(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlString *result = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  while (self->ptr < self->end && *self->ptr != '\"' && *self->ptr != '\n') {
    char ch1 = *self->ptr;
    if (ch1 == '\\') {
      if (self->ptr >= self->end) break;

      toml_move_next(self);
      char ch2 = *self->ptr;

      if (ch2 == '\"') {
        toml_string_append_char(result, '\"', &err);
      } else if (ch2 == 'b') {
        toml_string_append_char(result, '\b', &err);
      } else if (ch2 == 't') {
        toml_string_append_char(result, '\t', &err);
      } else if (ch2 == 'n') {
        toml_string_append_char(result, '\n', &err);
      } else if (ch2 == 'f') {
        toml_string_append_char(result, '\f', &err);
      } else if (ch2 == 'r') {
        toml_string_append_char(result, '\r', &err);
      } else if (ch2 == '"') {
        toml_string_append_char(result, '\"', &err);
      } else if (ch2 == '\\') {
        toml_string_append_char(result, '\\', &err);
      } else if (ch2 == 'u') {
        toml_encode_unicode_scalar16(result, self, &err);
      } else if (ch2 == 'U') {
        toml_encode_unicode_scalar32(result, self, &err);
      } else {
        toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid escape charactor");
        goto cleanup;
      }
    } else {
      toml_string_append_char(result, ch1, &err);
    }
    toml_move_next(self);

    if (err.code != TOML_OK) goto cleanup;
  }

  if (self->ptr >= self->end || *self->ptr != '\"' || *self->ptr == '\n') {
    toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated basic string",
                 self->filename, self->lineno, self->colno);
    goto cleanup;
  }

  toml_move_next(self);

cleanup:
  toml_err_move(error, &err);
  return result;
}

TomlString *toml_parse_literal_string(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlString *result = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  while (self->ptr < self->end && *self->ptr != '\'' && *self->ptr != '\n') {
    toml_string_append_char(result, *self->ptr, &err);
    if (err.code != TOML_OK) goto cleanup;
    toml_move_next(self);
  }

  if (self->ptr >= self->end || *self->ptr != '\"' || *self->ptr == '\n') {
    toml_set_err(error, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated literal string",
                 self->filename, self->lineno, self->colno);
  }

  toml_move_next(self);

cleanup:
  toml_err_move(error, &err);
  return result;
}

TomlValue *toml_parse_basic_string_value(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *value = NULL;

  TomlString *string = toml_parse_basic_string(self, &err);
  if (err.code != TOML_OK) goto cleanup;

  value = toml_value_new(TOML_STRING, &err);
  if (err.code != TOML_OK) goto cleanup;

  value->value.string = string;

cleanup:
  toml_err_move(error, &err);
  return value;
}

TomlValue *toml_parse_literal_string_value(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlValue *value = NULL;

  TomlString *string = toml_parse_literal_string(self, &err);
  if (err.code != TOML_OK) goto cleanup;

  value = toml_value_new(TOML_STRING, &err);
  if (err.code != TOML_OK) goto cleanup;

  value->value.string = string;

cleanup:
  toml_err_move(error, &err);
  return value;
}

TomlValue *toml_parse_multi_line_basic_string(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlValue *value = NULL;

  TomlString *result = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

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
        toml_string_append_char(result, '\"', &err);
      } else if (ch2 == 'b') {
        toml_string_append_char(result, '\b', &err);
      } else if (ch2 == 't') {
        toml_string_append_char(result, '\t', &err);
      } else if (ch2 == 'n') {
        toml_string_append_char(result, '\n', &err);
      } else if (ch2 == 'f') {
        toml_string_append_char(result, '\f', &err);
      } else if (ch2 == 'r') {
        toml_string_append_char(result, '\r', &err);
      } else if (ch2 == '"') {
        toml_string_append_char(result, '\"', &err);
      } else if (ch2 == '\\') {
        toml_string_append_char(result, '\\', &err);
      } else if (ch2 == 'u') {
        toml_encode_unicode_scalar16(result, self, &err);
      } else if (ch2 == 'U') {
        toml_encode_unicode_scalar32(result, self, &err);
      } else if (ch2 == '\n') {
        toml_move_next(self);
        while (self->ptr + 3 <= self->end &&
               (*self->ptr == ' ' || *self->ptr == '\t' || *self->ptr == '\r')) {
          toml_move_next(self);
        }
      } else {
        toml_string_append_char(result, ch1, &err);
        if (err.code != TOML_OK) goto cleanup;

        toml_string_append_char(result, ch2, &err);
        if (err.code != TOML_OK) goto cleanup;
      }
    } else {
      if (ch1 == '\n') {
        self->lineno++;
        self->colno = 1;
      }

      toml_string_append_char(result, ch1, &err);
      if (err.code != TOML_OK) goto cleanup;
    }

    toml_move_next(self);
  }

  if (self->ptr + 3 > self->end || strncmp(self->ptr, "\"\"\"", 3) != 0) {
    toml_set_err(&err, TOML_ERR_SYNTAX,
                 "%s:%d:%d: unterminated multi-line basic string",
                 self->filename, self->lineno, self->colno);
    goto cleanup;
  }

  toml_move_next(self);
  toml_move_next(self);
  toml_move_next(self);

  value = toml_value_new(TOML_STRING, &err);
  if (err.code != TOML_OK) goto cleanup;

  value->value.string = result;

cleanup:
  toml_err_move(error, &err);
  return value;
}

TomlValue *toml_parse_multi_line_literal_string(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlValue *value = NULL;

  TomlString *result = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  if (*self->ptr == '\n') {
    toml_move_next(self);
  }

  while (self->ptr + 3 <= self->end && strncmp(self->ptr, "\'\'\'", 3) != 0) {
    toml_string_append_char(result, *self->ptr, &err);
    if (err.code != TOML_OK) goto cleanup;
    toml_move_next(self);
  }

  if (self->ptr + 3 > self->end || strncmp(self->ptr, "\'\'\'", 3) != 0) {
    toml_set_err(&err, TOML_ERR_SYNTAX,
                 "%s:%d:%d: unterminated multi-line literal string",
                 self->filename, self->lineno, self->colno);
    goto cleanup;
  }

  toml_move_next(self);
  toml_move_next(self);
  toml_move_next(self);

  value = toml_value_new(TOML_STRING, &err);
  if (err.code != TOML_OK) goto cleanup;

  value->value.string = result;

cleanup:
  toml_err_move(error, &err);
  return value;
}

TomlValue *toml_parse_datetime(const char *str, size_t len, TomlErr *error)
{
  (void)str;
  (void)len;
  return toml_value_new(TOML_DATETIME, error);
}

TomlValue *toml_parse_int_or_float_or_time(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlString *str = NULL;
  TomlValue *result = NULL;

  char type = 'i';

  str = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  if (self->ptr + 3 <= self->end &&
      (strncmp(self->ptr, "nan", 3) == 0 || strncmp(self->ptr, "inf", 3) == 0)) {
    toml_string_append_nstring(str, self->ptr, 3, &err);
    if (err.code != TOML_OK) goto cleanup;
    type = 'f';
  } else if (self->ptr + 4 <= self->end &&
             (strncmp(self->ptr, "+nan", 4) == 0 ||
              strncmp(self->ptr, "-nan", 4) == 0 ||
              strncmp(self->ptr, "+inf", 4) == 0 ||
              strncmp(self->ptr, "-inf", 4) == 0)) {
    toml_string_append_nstring(str, self->ptr, 4, &err);
    if (err.code != TOML_OK) goto cleanup;
    type = 'f';
  } else {

    char last_char = 0;
    bool has_exp = false;
    while (self->ptr < self->end) {
      if (*self->ptr == '+' || *self->ptr == '-') {
        if (last_char == 0 || ((last_char == 'e' || last_char == 'E') && !has_exp)) {
          if (last_char != 0) {
            type = 'f';
            has_exp = true;
          }
          toml_string_append_char(str, *self->ptr, &err);
          if (err.code != TOML_OK) goto cleanup;
        } else {
          break;
        }
      } else if (isalnum(*self->ptr)) {
        toml_string_append_char(str, *self->ptr, &err);
        if (err.code != TOML_OK) goto cleanup;
      } else if (*self->ptr == '.') {
        if (type == 'i') {
          type = 'f';
          toml_string_append_char(str, *self->ptr, &err);
          if (err.code != TOML_OK) goto cleanup;
        } else {
          toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid float",
                       self->filename, self->lineno, self->colno);
          goto cleanup;
        }
      } else if (*self->ptr == '_') {
        if (type == 't') {
          toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid datetime",
                       self->filename, self->lineno, self->colno);
          goto cleanup;
        }

        if (!isalnum(last_char)) {
          toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid integer or float",
                       self->filename, self->lineno, self->colno);
          goto cleanup;
        }
      } else if (*self->ptr == '-') {
        type = 't';
        toml_string_append_char(str, *self->ptr, &err);
      } else {
        break;
      }

      last_char = *self->ptr;
      toml_move_next(self);
    }

    if (last_char == '_') {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid integer or float or datetime",
                   self->filename, self->lineno, self->colno);
      goto cleanup;
    }
  }

  if (type == 'i') {
    char *end = NULL;
    int64_t n = strtoll(str->str, &end, 0);
    if (end < str->str + str->len) {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid integer");
      goto cleanup;
    }
    result = toml_value_new_integer(n, &err);
    if (err.code != TOML_OK) goto cleanup;
  } else if (type == 'f') {
    char *end = NULL;
    double n = strtod(str->str, &end);
    if (end < str->str + str->len) {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid float");
      goto cleanup;
    }
    result = toml_value_new_float(n, &err);
    if (err.code != TOML_OK) goto cleanup;
  } else if (type == 't') {
    result = toml_parse_datetime(str->str, str->len, &err);
    if (err.code != TOML_OK) goto cleanup;
  }

cleanup:
  toml_string_free(str);
  toml_err_move(error, &err);
  return result;
}

TomlValue *toml_parse_bool(TomlParser *self, TomlErr *error)
{
  if (self->ptr + 4 <= self->end && strncmp(self->ptr, "true", 4) == 0 &&
      (self->ptr + 4 == self->end || isspace(*(self->ptr + 4)))) {
    toml_move_next(self);
    toml_move_next(self);
    toml_move_next(self);
    toml_move_next(self);
    return toml_value_new_boolean(true, error);
  }

  if (self->ptr + 5 <= self->end && strncmp(self->ptr, "false", 5) == 0 &&
      (self->ptr + 5 == self->end || isspace(*(self->ptr + 5)))) {
    toml_move_next(self);
    toml_move_next(self);
    toml_move_next(self);
    toml_move_next(self);
    toml_move_next(self);
    return toml_value_new_boolean(false, error);
  }

  return NULL;
}

TomlValue *toml_parse_array(TomlParser *self, TomlErr *error);
TomlValue *toml_parse_inline_table(TomlParser *self, TomlErr *error);

TomlValue *toml_parse_value(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlValue *value = NULL;

  char ch = *self->ptr;

  if (strncmp(self->ptr, "\"\"\"", 3) == 0) {
    toml_move_next(self);
    toml_move_next(self);
    toml_move_next(self);
    value = toml_parse_multi_line_basic_string(self, &err);
  } else if (strncmp(self->ptr, "\'\'\'", 3) == 0) {
    toml_move_next(self);
    toml_move_next(self);
    toml_move_next(self);
    value = toml_parse_multi_line_literal_string(self, &err);
  } else if (ch == '\"') {
    toml_move_next(self);
    value = toml_parse_basic_string_value(self, &err);
  } else if (ch == '\'') {
    toml_move_next(self);
    value = toml_parse_literal_string_value(self, &err);
  } else if (isdigit(ch) || ch == '+' || ch == '-' || ch == '.' || ch == 'n' || ch == 'i') {
    value = toml_parse_int_or_float_or_time(self, &err);
  } else if (ch == 't' || ch == 'f') {
    value = toml_parse_bool(self, &err);
  } else if (ch == '[') {
    toml_move_next(self);
    value = toml_parse_array(self, &err);
  } else if (ch == '{') {
    toml_move_next(self);
    value = toml_parse_inline_table(self, &err);
  } else {
    toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                 self->filename, self->lineno, self->colno);
  }

  toml_err_move(error, &err);
  return value;
}

void toml_parse_key_value(TomlParser *self, TomlTable *table, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  while (self->ptr < self->end) {
    TomlString *key = NULL;
    TomlValue *value = NULL;
    char ch;

    ch = *self->ptr;
    while (self->ptr < self->end && isspace(ch)) {
      toml_move_next(self);
      ch = *self->ptr;
    }

    if (self->ptr == self->end) break;

    if (isalnum(ch) || ch == '_') {
      key = toml_parse_bare_key(self, &err);
    } else if (ch == '\"') {
      toml_move_next(self);
      key = toml_parse_basic_string(self, &err);
    } else if (ch == '\'') {
      toml_move_next(self);
      key = toml_parse_literal_string(self, &err);
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
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                   self->filename, self->lineno, self->colno);
      goto cleanup;
    }

    ch = *self->ptr;
    while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
      toml_move_next(self);
      ch = *self->ptr;
    }

    if (self->ptr == self->end) {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
      goto cleanup;
    }

    if (ch != '=') {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                   self->filename, self->lineno, self->colno);
      goto cleanup;
    }

    toml_move_next(self);

    ch = *self->ptr;
    while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
      toml_move_next(self);
      ch = *self->ptr;
    }

    if (self->ptr == self->end) {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
      goto cleanup;
    }

    value = toml_parse_value(self, &err);
    if (err.code != TOML_OK) goto cleanup;

    toml_table_set(table, key, value, &err);
    if (err.code != TOML_OK) goto cleanup;

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
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: new line expected",
                   self->filename, self->lineno, self->colno);
      goto cleanup;
    }
  }

cleanup:
  toml_err_move(error, &err);
}

TomlValue *toml_parse_array(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlValue *array = NULL;

  array = toml_value_new_array(&err);
  if (err.code != TOML_OK) goto cleanup;

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
    } else if (*self->ptr == ',') {
      toml_move_next(self);
    } else if (*self->ptr == ']') {
      toml_move_next(self);
      break;
    } else {
      TomlValue *value = toml_parse_value(self, &err);
      if (err.code != TOML_OK) goto cleanup;

      toml_array_append(array->value.array, value, &err);
      if (err.code != TOML_OK) goto cleanup;
    }
  }

cleanup:
  toml_err_move(error, &err);
  return array;
}

TomlValue *toml_parse_inline_table(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlValue *table = NULL;

  table = toml_value_new_table(&err);
  if (err.code != TOML_OK) goto cleanup;

  while (self->ptr < self->end) {
    TomlString *key = NULL;
    TomlValue *value = NULL;
    char ch;

    ch = *self->ptr;
    while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
      toml_move_next(self);
      ch = *self->ptr;
    }

    if (isalnum(ch) || ch == '_' || ch == '-') {
      key = toml_parse_bare_key(self, &err);
    } else if (ch == '\"') {
      toml_move_next(self);
      key = toml_parse_basic_string(self, &err);
    } else if (ch == '\'') {
      toml_move_next(self);
      key = toml_parse_literal_string(self, &err);
    } else if (ch == '}') {
      break;
    } else {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                   self->filename, self->lineno, self->colno);
      goto cleanup;
    }

    ch = *self->ptr;
    while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
      toml_move_next(self);
      ch = *self->ptr;
    }

    if (self->ptr == self->end) {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
      goto cleanup;
    }

    if (ch != '=') {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unexpected token",
                   self->filename, self->lineno, self->colno);
      goto cleanup;
    }

    toml_move_next(self);

    ch = *self->ptr;
    while (self->ptr < self->end && (ch == ' ' || ch == '\t')) {
      toml_move_next(self);
      ch = *self->ptr;
    }

    if (self->ptr == self->end) {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated key value pair");
      goto cleanup;
    }

    value = toml_parse_value(self, &err);
    if (err.code != TOML_OK) goto cleanup;

    toml_table_set(table->value.table, key, value, &err);
    if (err.code != TOML_OK) goto cleanup;

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

cleanup:
  toml_err_move(error, &err);
  return table;
}

TomlTable *toml_walk_table_path(TomlParser *parser, TomlTable *table,
                                TomlArray *key_path, bool is_array,
                                TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlTable *real_table = table;

  if (is_array) {
    size_t i = 0;
    for (; i < key_path->len - 1; i++) {
      TomlString *part = key_path->elements[i]->value.string;
      TomlValue *t = toml_table_get(real_table, part);
      if (t == NULL) {
        TomlValue *new_table = toml_value_new_table(&err);
        if (err.code != TOML_OK) goto cleanup;

        toml_table_set(real_table, part, new_table, &err);
        if (err.code != TOML_OK) goto cleanup;

        real_table = new_table->value.table;
      } else {
        real_table = t->value.table;
      }
    }

    TomlString *part = key_path->elements[i]->value.string;
    TomlValue *t = toml_table_get(real_table, part);
    if (t == NULL) {
      TomlValue *array = toml_value_new_array(&err);
      if (err.code != TOML_OK) goto cleanup;

      TomlValue *new_table = toml_value_new_table(&err);
      if (err.code != TOML_OK) goto cleanup;

      toml_array_append(array->value.array, new_table, &err);
      if (err.code != TOML_OK) goto cleanup;

      toml_table_set(real_table, part, array, &err);
      if (err.code != TOML_OK) goto cleanup;

      real_table = new_table->value.table;
    } else {
      if (t->type != TOML_ARRAY) {
        toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: this key was not an array",
                     parser->filename, parser->lineno, parser->colno);
        goto cleanup;
      }

      TomlValue *new_table = toml_value_new_table(&err);
      if (err.code != TOML_OK) goto cleanup;

      toml_array_append(t->value.array, new_table, &err);
      if (err.code != TOML_OK) goto cleanup;

      real_table = new_table->value.table;
    }
  } else {
    for (size_t i = 0; i < key_path->len; i++) {
      TomlString *part = key_path->elements[i]->value.string;
      TomlValue *t = toml_table_get(real_table, part);
      if (t == NULL) {
        TomlValue *new_table = toml_value_new_table(&err);
        if (err.code != TOML_OK) goto cleanup;

        toml_table_set(real_table, part, new_table, &err);
        if (err.code != TOML_OK) goto cleanup;

        real_table = new_table->value.table;
      } else {
        if (t->type == TOML_ARRAY) {
          real_table = t->value.array->elements[t->value.array->len - 1]->value.table;
        } else if (t->type == TOML_TABLE) {
          real_table = t->value.table;
        }
      }
    }
  }

cleanup:
  toml_err_move(error, &err);
  return real_table;
}

void toml_parse_table(TomlParser *self, TomlTable *table, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlArray *key_path = NULL;
  bool is_array = false;
  TomlTable *real_table = table;

  key_path = toml_array_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  if (self->ptr < self->end && *self->ptr == '[') {
    is_array = true;
    toml_move_next(self);
  }

  while (1) {
    TomlString *key_part = NULL;
    TomlValue *key_part_value = NULL;

    if (is_array) {
      if (self->ptr + 2 <= self->end && *self->ptr != '\n') {
        if (strncmp(self->ptr, "]]", 2) == 0) {
          toml_move_next(self);
          toml_move_next(self);
          break;
        }
      } else {
        toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated name of array of tables",
                     self->filename, self->lineno, self->colno);
        goto cleanup;
      }
    } else {
      if (self->ptr < self->end && *self->ptr != '\n') {
        if (*self->ptr == ']') {
          toml_move_next(self);
          break;
        }
      } else {
        toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: unterminated table name",
                     self->filename, self->lineno, self->colno);
        goto cleanup;
      }
    }

    if (isalnum(*self->ptr) || *self->ptr == '_') {
      key_part = toml_parse_bare_key(self, &err);
    } else if (*self->ptr == '\"') {
      toml_move_next(self);
      key_part = toml_parse_basic_string(self, &err);
    } else if (*self->ptr == '\'') {
      toml_move_next(self);
      key_part = toml_parse_literal_string(self, &err);
    } else {
      toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: invalid token",
                   self->filename, self->lineno, self->colno);
    }
    if (err.code != TOML_OK) goto cleanup;

    key_part_value = toml_value_new(TOML_STRING, &err);
    if (err.code != TOML_OK) goto cleanup;

    key_part_value->value.string = key_part;

    toml_array_append(key_path, key_part_value, &err);
    if (err.code != TOML_OK) goto cleanup;

    if (self->ptr >= self->end) break;

    if (*self->ptr == '.') {
      toml_move_next(self);
    }
  }

  if (key_path->len == 0) {
    toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: empty table name",
                 self->filename, self->lineno, self->colno);
    goto cleanup;
  }

  while (self->ptr < self->end &&
         (*self->ptr == ' ' || *self->ptr == '\t' || *self->ptr == '\r')) {
    toml_move_next(self);
  }

  if (self->ptr < self->end && *self->ptr != '\n') {
    toml_set_err(&err, TOML_ERR_SYNTAX, "%s:%d:%d: new line expected",
                 self->filename, self->lineno, self->colno);
    goto cleanup;
  }

  real_table = toml_walk_table_path(self, table, key_path, is_array, &err);
  if (err.code != TOML_OK) goto cleanup;

  toml_parse_key_value(self, real_table, &err);

cleanup:
  toml_err_move(error, &err);
}

TomlTable *toml_parse(TomlParser *self, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;

  TomlTable *table = NULL;

  table = toml_table_new(&err);
  if (err.code != TOML_OK) goto cleanup;

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
      toml_parse_table(self, table, &err);
      if (err.code != TOML_OK) goto cleanup;
    } else if (isalnum(ch) || ch == '_' || ch == '-') {
      toml_parse_key_value(self, table, &err);
      if (err.code != TOML_OK) goto cleanup;
    } else if (ch == ' ' || ch == '\t' || ch == '\r') {
      do {
        toml_move_next(self);
        ch = *self->ptr;
      } while (ch == ' ' || ch == '\t' || ch == '\r');
    } else if (ch == '\n') {
      toml_move_next(self);
    }
  }

cleanup:
  if (err.code != TOML_OK) {
    toml_table_free(table);
  }
  toml_err_move(error, &err);
  return table;
}

TomlTable *toml_load_nstring(const char *str, size_t len, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlParser *parser = NULL;
  TomlTable *table = NULL;

  parser = toml_parser_new(str, len, &err);
  if (err.code != TOML_OK) goto cleanup;

  table = toml_parse(parser, &err);
  if (err.code != TOML_OK) goto cleanup;

cleanup:
  toml_err_move(error, &err);
  return table;
}

TomlTable *toml_load_string(const char *str, TomlErr *error)
{
  return toml_load_nstring(str, sizeof(str), error);
}

TomlTable *toml_load_file(FILE *file, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlTable *table = NULL;
  TomlString *str = NULL;

  str = toml_string_new(&err);
  if (err.code != TOML_OK) goto cleanup;

  toml_string_check_expand(str, 4095, &err);
  if (err.code != TOML_OK) goto cleanup;

  size_t count;
  size_t bytes_to_read;
  do {
    bytes_to_read = str->_capacity - str->len - 1;

    count = fread(str->str, 1, bytes_to_read, file);

    if (str->len + 1 >= str->_capacity) {
      toml_string_check_expand(str, str->_capacity * 2, &err);
      if (err.code != TOML_OK) goto cleanup;
    }

    str->len += count;
  } while (count == bytes_to_read);

  str->str[str->len] = 0;

  if (ferror(file)) {
    toml_set_err_literal(&err, TOML_ERR_IO, "error when reading file");
    goto cleanup;
  }

  table = toml_load_nstring(str->str, str->len, &err);

cleanup:
  toml_string_free(str);
  toml_err_move(error, &err);
  return table;
}

TomlTable *toml_load_filename(const char *filename, TomlErr *error)
{
  TomlErr err = TOML_ERR_INIT;
  TomlTable *table = NULL;
  FILE *f = NULL;

  f = fopen(filename, "rb");
  if (f == NULL) {
    toml_set_err(&err, TOML_ERR_IO, "cannot open file: %s", filename);
    goto cleanup;
  }

  table = toml_load_file(f, &err);

cleanup:
  if (f != NULL) fclose(f);
  toml_err_move(error, &err);
  return table;
}
