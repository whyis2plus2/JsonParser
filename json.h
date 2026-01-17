#include <ctype.h>
#include <malloc.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h> 

#include <math.h> // needed for signbit, isnan, isinf, INFINITY, NAN

#ifndef JSON_H_
#define JSON_H_ 1

#define JSON_TRUE  1
#define JSON_FALSE 0

typedef int Json_Type;
enum Json_Type_Enum {
  JSON_TYPE_NULL,
  JSON_TYPE_BOOLEAN,
  JSON_TYPE_NUMBER,
  JSON_TYPE_STRING,
  JSON_TYPE_ARRAY,
  JSON_TYPE_OBJECT
};

typedef struct Json_Value Json_Value;
typedef int    Json_Boolean;
typedef double Json_Number;

typedef struct {
  Json_Boolean is_heap;
  size_t len;
  char *data;
} Json_String;

#ifdef __cplusplus
# if __cplusplus >= 201103l 
#   define JSON_STRLIT(s) Json_String {JSON_FALSE, sizeof(s) - 1, (char *)(void *)"" s ""}
# else
static inline
Json_String Json__impl_strLit(size_t len, const char *s)
{
  Json_String ret;
  ret.is_heap = JSON_FALSE;
  ret.data = (char *)(void *)s;
  ret.len = len;
  return ret;
}
#   define JSON_STRLIT(s) Json__impl_strLit(sizeof(s) - 1, "" s "")
# endif
#else
# define JSON_STRLIT(s) (Json_String){JSON_FALSE, sizeof(s) - 1, "" s ""}
#endif

typedef struct {
  size_t len;
  size_t cap;
  Json_Value *elems;
} Json_Array;

typedef struct {
  size_t len;
  size_t cap;
  Json_String *field_names;
  Json_Value  *field_values;
} Json_Object;

struct Json_Value {
  Json_Type type;
  union {
    Json_Boolean as_boolean;
    Json_Number  as_number;
    Json_String  as_string;
    Json_Array   as_array;
    Json_Object  as_object;
  } v;
};

int Json_stringCmp(Json_String a, Json_String b);

Json_Value *Json_asValue(Json_Value *out, Json_Type type, ...);
void Json_destroyValue(Json_Value *value);

void Json_arrayAppend(Json_Value *array, const Json_Value *src);
void Json_arrayDelete(Json_Value *array, size_t idx);
void Json_destroyArray(Json_Value *array);

void Json_objectSet(Json_Value *object, Json_String field, const Json_Value *src);
void Json_objectSetNull(Json_Value *object, Json_String field);
void Json_objectSetBool(Json_Value *object, Json_String field, Json_Boolean val);
void Json_objectSetNum(Json_Value *object, Json_String field, Json_Number val);
void Json_objectSetStr(Json_Value *object, Json_String field, Json_String val);

Json_Value   *Json_objectGet(Json_Value *object, Json_String field);
Json_Boolean  Json_objectGetBool(Json_Value *object, Json_String field, Json_Boolean fallback);
Json_Number   Json_objectGetNum(Json_Value *object, Json_String field, Json_Number fallback);
Json_String   Json_objectGetStr(Json_Value *object, Json_String field, Json_String fallback);

void Json_objectDelete(Json_Value *object, Json_String field);
void Json_destroyObject(Json_Value *object);

void Json_printValue(FILE *file, const Json_Value *value);
size_t Json_parseFile(FILE *file, Json_Value *out);
size_t Json_parseStr(size_t buf_sz, const char *buffer, Json_Value *out);

#endif // !JSON_H_

#ifdef JSON_IMPLEMENTATION

int Json_stringCmp(Json_String a, Json_String b)
{
  if (!a.data && !b.data) return 0;
  if (!a.data) return -1;
  if (!b.data) return  1;

  const int cmp = memcmp(a.data, b.data, (a.len < b.len)? a.len : b.len);
  if (!cmp) {
    return (a.len < b.len)? -1 :
           (a.len > b.len)?  1 : 0;
  }

  return cmp;
}

Json_Value *Json_asValue(Json_Value *out, Json_Type type, ...)
{
  if (!out) return NULL;
  memset(out, 0, sizeof(*out));
  out->type = type;

  va_list args;
  va_start(args, type);

  switch (type) {
    case JSON_TYPE_NULL: break;

    case JSON_TYPE_BOOLEAN:
      out->v.as_boolean = va_arg(args, Json_Boolean);
      break;

    case JSON_TYPE_NUMBER:
      out->v.as_number = va_arg(args, Json_Number);
      break;

    case JSON_TYPE_STRING:
      out->v.as_string = va_arg(args, Json_String);
      break;

    case JSON_TYPE_ARRAY:
      out->v.as_array = va_arg(args, Json_Array);
      break;

    case JSON_TYPE_OBJECT:
      out->v.as_object = va_arg(args, Json_Object);
      break;
  }

  va_end(args);
  return out;
}

void Json_destroyValue(Json_Value *value)
{
  if (!value) return;

  switch (value->type) {
    case JSON_TYPE_STRING:
      if (value->v.as_string.is_heap) {
        free(value->v.as_string.data);
      }
      break;

    case JSON_TYPE_ARRAY:  Json_destroyArray(value);  break;
    case JSON_TYPE_OBJECT: Json_destroyObject(value); break;
    default: break;
  }

  memset(value, 0, sizeof(*value));
}

void Json_arrayAppend(Json_Value *_array, const Json_Value *src)
{
  if (!_array || !src) return;
  if (_array->type != JSON_TYPE_ARRAY) return;

  Json_Array *array = &_array->v.as_array;
  if (array->len + 1 > array->cap) {
    array->cap = (array->cap < 128)? 128 : 2 * array->cap;
    array->elems = (Json_Value *)realloc(array->elems, sizeof(Json_Value) * array->cap);
    if (!array->elems) return; // TODO: actually do some error handling here
  }

  array->elems[array->len] = *src;
  ++array->len;
}

void Json_arrayDelete(Json_Value *_array, size_t idx)
{
  if (!_array) return;
  if (_array->type != JSON_TYPE_ARRAY) return;

  Json_Array *array = &_array->v.as_array;
  if (idx > array->len) return;

  memmove(
    &array->elems[idx],
    &array->elems[idx + 1],
    sizeof(*array->elems) * (array->len - idx - 1)
  );

  --array->len;
}

void Json_destroyArray(Json_Value *array)
{
  if (!array) return;
  if (array->type != JSON_TYPE_ARRAY) return;

  for (size_t i = 0; i < array->v.as_array.len; ++i) {
    Json_destroyValue(&array->v.as_array.elems[i]);
  }

  free(array->v.as_array.elems);
  array->type = JSON_TYPE_NULL;
  array->v.as_array.cap = array->v.as_array.len = 0;
}

void Json_objectSet(Json_Value *_object, Json_String field, const Json_Value *src)
{
  if (!_object || !field.data || !src) return;
  if (_object->type != JSON_TYPE_OBJECT) return;

  Json_Object *object = &_object->v.as_object;
  if (object->len + 1 > object->cap) {
    object->cap = (object->cap < 128)? 128 : 2 * object->cap;
    object->field_names = (Json_String *)realloc(
      object->field_names, sizeof(*object->field_names) * object->cap
    );

    object->field_values = (Json_Value *)realloc(
      object->field_values, sizeof(*object->field_values) * object->cap
    );

    if (!object->field_names || !object->field_values) return; // TODO: error handling
  }

  for (size_t i = 0; i < object->len; ++i) {
    if (Json_stringCmp(object->field_names[i], field)) continue;
    Json_destroyObject(&object->field_values[i]);
    object->field_values[i] = *src;
    return;
  }

  object->field_names[object->len]  = field;
  object->field_values[object->len] = *src;
  ++object->len;
}

void Json_objectSetNull(Json_Value *object, Json_String field)
{
  static const Json_Value null = {JSON_TYPE_NULL, {0}};
  Json_objectSet(object, field, &null);
}

void Json_objectSetBool(Json_Value *object, Json_String field, Json_Boolean val)
{
  Json_Value tmp;
  Json_objectSet(object, field, Json_asValue(&tmp, JSON_TYPE_BOOLEAN, val));
}

void Json_objectSetNum(Json_Value *object, Json_String field, Json_Number val)
{
  Json_Value tmp;
  Json_objectSet(object, field, Json_asValue(&tmp, JSON_TYPE_NUMBER, val));
}

void Json_objectSetStr(Json_Value *object, Json_String field, Json_String val)
{
  Json_Value tmp;
  Json_objectSet(object, field, Json_asValue(&tmp, JSON_TYPE_STRING, val));
}

Json_Value *Json_objectGet(Json_Value *_object, Json_String field)
{
  if (!_object || !field.data) return NULL;
  if (_object->type != JSON_TYPE_OBJECT) return NULL;

  const Json_Object *object = &_object->v.as_object;
  for (size_t i = 0; i < object->len; ++i) {
    if (Json_stringCmp(object->field_names[i], field)) continue;
    return &object->field_values[i];
  }

  return NULL;
}

Json_Boolean Json_objectGetBool(Json_Value *object, Json_String field, Json_Boolean fallback)
{
  Json_Value *out = Json_objectGet(object, field);
  if (!out) return fallback;

  switch (out->type) {
    case JSON_TYPE_NULL: return JSON_FALSE;
    case JSON_TYPE_NUMBER:
      if ((int)out->v.as_number > 2) return fallback;
      return !!(int)out->v.as_number;

    case JSON_TYPE_STRING:
      if (!Json_stringCmp(JSON_STRLIT("true"), out->v.as_string)) return JSON_TRUE;
      if (!Json_stringCmp(JSON_STRLIT("false"), out->v.as_string)) return JSON_FALSE;
      return fallback;

    case JSON_TYPE_ARRAY:
      if (out->v.as_array.len != 1) return fallback;
      if (out->v.as_array.elems[0].type != JSON_TYPE_BOOLEAN) return fallback;
      return out->v.as_array.elems[0].v.as_boolean;
    
    case JSON_TYPE_OBJECT: return fallback;
    default: break;
  }

  return out->v.as_boolean;
}

Json_Number Json_objectGetNum(Json_Value *object, Json_String field, Json_Number fallback)
{
  Json_Value *out = Json_objectGet(object, field);
  if (!out) return fallback;

  switch (out->type) {
    case JSON_TYPE_NULL: return 0;
    case JSON_TYPE_BOOLEAN: return (Json_Number)out->v.as_boolean;

    case JSON_TYPE_STRING:
      if (!Json_stringCmp(JSON_STRLIT("Infinity"), out->v.as_string)) return INFINITY;
      if (!Json_stringCmp(JSON_STRLIT("-Infinity"), out->v.as_string)) return -INFINITY;
      if (!Json_stringCmp(JSON_STRLIT("NaN"), out->v.as_string)) return NAN;
      if (!Json_stringCmp(JSON_STRLIT("-NaN"), out->v.as_string)) return -NAN;
      return fallback;

    case JSON_TYPE_ARRAY:
      if (out->v.as_array.len != 1) return fallback;
      if (out->v.as_array.elems[0].type != JSON_TYPE_NUMBER) return fallback;
      return out->v.as_array.elems[0].v.as_number;
    
    case JSON_TYPE_OBJECT: return fallback;
    default: break;
  }

  return out->v.as_number;
}

Json_String Json_objectGetStr(Json_Value *object, Json_String field, Json_String fallback)
{
  Json_Value *out = Json_objectGet(object, field);
  if (!out) return fallback;

  // return value that will be used if the heap is needed
  Json_String ret = {JSON_TRUE, 0, NULL};

  switch (out->type) {
    case JSON_TYPE_NULL: return JSON_STRLIT("");
    case JSON_TYPE_BOOLEAN: return (out->v.as_boolean)? JSON_STRLIT("true") : JSON_STRLIT("false");
    case JSON_TYPE_NUMBER:
      ret.len = snprintf(NULL, 0, "%.16g", out->v.as_number);
      ret.data = calloc(1, ret.len);
      snprintf(ret.data, ret.len, "%.16g", out->v.as_number);
      return ret;

    case JSON_TYPE_ARRAY:
      if (out->v.as_array.len != 1) return fallback;
      if (out->v.as_array.elems[0].type != JSON_TYPE_STRING) return fallback;
      return out->v.as_array.elems[0].v.as_string;
    
    case JSON_TYPE_OBJECT: return fallback;
    default: break;
  }

  return out->v.as_string;
}

void Json_objectDelete(Json_Value *_object, Json_String field)
{
  if (!_object || !field.data) return;
  if (_object->type != JSON_TYPE_OBJECT) return;

  Json_Object *object = &_object->v.as_object;
  size_t i;
  for (i = 0; i < object->len; ++i) {
    if (Json_stringCmp(object->field_names[i], field)) continue;
    break;
  }

  if (i >= object->len) return;

  if (object->field_names[i].is_heap) free(object->field_names[i].data);
  Json_destroyValue(&object->field_values[i]);

  if (i < object->len - 1) {
    memmove(
      &object->field_names[i],
      &object->field_names[i + 1],
      sizeof(Json_String) * (object->len - i - 1)
    );

    memmove(
      &object->field_values[i],
      &object->field_values[i + 1],
      sizeof(Json_Value) * (object->len - i - 1)
    );
  }

  --object->len;
}

void Json_destroyObject(Json_Value *object)
{
  if (!object) return;
  if (object->type != JSON_TYPE_OBJECT) return;

  for (size_t i = 0; i < object->v.as_object.len; ++i) {
    if (object->v.as_object.field_names[i].is_heap) {
      free(object->v.as_object.field_names[i].data);
    }
    Json_destroyValue(&object->v.as_object.field_values[i]);
  }

  free(object->v.as_object.field_names);
  free(object->v.as_object.field_values);
  object->type = JSON_TYPE_NULL;
  object->v.as_object.len = object->v.as_object.cap = 0;
}

static
void Json__realPrintValue(
  FILE *file,
  const Json_Value *value,
  int ntabs,
  Json_Boolean omit_start_tabs
)
{
  if (!file || !value) return;

  if (!omit_start_tabs) {
    for (int i = 0; i < ntabs; ++i) putc('\t', file);
  }

  switch (value->type) {
    case JSON_TYPE_NULL: fprintf(file, "null"); break;

    case JSON_TYPE_BOOLEAN:
      fprintf(file, (value->v.as_boolean)? "true" : "false");
      break;

    case JSON_TYPE_NUMBER: {
      const char *str = isinf(value->v.as_number)? "-Infinity" :
                        isnan(value->v.as_number)? "-NaN"      : NULL;

      if (str) {
        if (!signbit(value->v.as_number)) ++str;
        Json_Value tmp;
        Json_String jstr = {JSON_FALSE, strlen(str), (char *)(void *)str};
        Json_asValue(&tmp, JSON_TYPE_STRING, jstr);

        Json__realPrintValue(file, &tmp, ntabs, JSON_TRUE);
        break;
      }

      fprintf(file, "%.16g", value->v.as_number);
    } break;

    case JSON_TYPE_STRING:
      putc('"', file);

      for (size_t i = 0; i < value->v.as_string.len; ++i) {
        unsigned long codepoint = ((unsigned long)value->v.as_string.data[i]) & 0xff;

        if (codepoint <= 0x7f) {
          switch ((char)codepoint) {
            case '"':  fprintf(file, "\\\""); continue;
            case '\\': fprintf(file, "\\\\"); continue;
            case '/':  fprintf(file, "\\/");  continue;
            case '\b': fprintf(file, "\\b");  continue;
            case '\f': fprintf(file, "\\f");  continue;
            case '\n': fprintf(file, "\\n");  continue;
            case '\r': fprintf(file, "\\r");  continue;
            case '\t': fprintf(file, "\\t");  continue;
            default: break;
          }

          if (!isprint(codepoint)) {
            fprintf(file, "\\u%.4lx", codepoint);
            continue;
          }

          fprintf(file, "%c", (char)codepoint);
          continue;
        }

        if (codepoint >> 5 == 6) {
          codepoint &= 0x1f;
          unsigned char byte = ((unsigned char)value->v.as_string.data[++i]);
          codepoint = (codepoint << 6) | (byte & 0x3f);
          fprintf(file, "\\u%.4lx", codepoint);

          continue;
        }

        if (codepoint >> 4 == 0xc) {
          codepoint &= 0xf;
          unsigned char byte = ((unsigned char)value->v.as_string.data[++i]);
          codepoint = (codepoint << 6) | (byte & 0x3f);
          byte = ((unsigned char)value->v.as_string.data[++i]);
          codepoint = (codepoint << 6) | (byte & 0x3f);
          fprintf(file, "\\u%.4lx", codepoint);

          continue;
        }

        if (codepoint >> 3 == 0x1e) {
          codepoint &= 7;
          unsigned char byte = ((unsigned char)value->v.as_string.data[++i]);
          codepoint = (codepoint << 6) | (byte & 0x3f);
          byte = ((unsigned char)value->v.as_string.data[++i]);
          codepoint = (codepoint << 6) | (byte & 0x3f);
          byte = ((unsigned char)value->v.as_string.data[++i]);
          codepoint = (codepoint << 6) | (byte & 0x3f);

          // invalid character
          if (codepoint > 0x10FFFF) {
            fprintf(file, "\\ufffd");
            continue;
          }

          // json uses utf-16 surrogates for characters outside the BMP
          // convert the codepoint to these values and print them
          codepoint -= 0x10000;
          unsigned short lo, hi;
          hi = 0xd800 + (codepoint >> 10);
          lo = 0xdc00 + (codepoint & 0x3ff);

          fprintf(file, "\\u%.4x\\u%.4x", hi, lo);
          continue;
        }

        fprintf(file, "\\ufffd");
        continue;
      }

      putc('"', file);
      break;

    case JSON_TYPE_ARRAY:
      fprintf(file, "[");

      for (size_t i = 0; i < value->v.as_array.len; ++i) {
        if (i) fprintf(file, ",");
        fprintf(file, "\n");

        Json__realPrintValue(file, &value->v.as_array.elems[i], ntabs + 1, JSON_FALSE);
      }

      fprintf(file, "\n");
      for (int i = 0; i < ntabs; ++i) putc('\t', file);
      fprintf(file, "]");
      break;

    case JSON_TYPE_OBJECT:
      fprintf(file, "{");

      for (size_t i = 0; i < value->v.as_object.len; ++i) {
        if (i) fprintf(file, ",");
        fprintf(file, "\n");

        Json_Value tmp;
        Json__realPrintValue(
          file,
          Json_asValue(&tmp, JSON_TYPE_STRING, value->v.as_object.field_names[i]),
          ntabs + 1,
          JSON_FALSE
        );

        fprintf(file, ": ");

        Json__realPrintValue(file, &value->v.as_object.field_values[i], ntabs + 1, JSON_TRUE);
      }

      fprintf(file, "\n");
      for (int i = 0; i < ntabs; ++i) putc('\t', file);
      fprintf(file, "}");
      break;
  }
}

void Json_printValue(FILE *file, const Json_Value *value)
{
  Json__realPrintValue(file, value, 0, JSON_TRUE);
  fprintf(file, "\n");
}

size_t Json_parseFile(FILE *file, Json_Value *out)
{
  if (!file || !out) return 0;

  const long start = ftell(file);
  fseek(file, 0, SEEK_END);
  const long buf_sz = ftell(file) - start + 1;
  if (buf_sz <= 1l || buf_sz > 1073741823l) return 0;

  char *buffer = (char *)calloc(1, buf_sz);
  if (!buffer) return 0;

  fseek(file, start, SEEK_SET);
  fread(buffer, 1, buf_sz, file);
  const size_t ret = Json_parseStr(buf_sz, buffer, out);

  free(buffer);
  return ret;
}

size_t Json_parseStr(size_t buf_sz, const char *buffer, Json_Value *out)
{
  size_t ret = 0;

  if (!buffer || !buf_sz || !out) return 0;
  memset(out, 0, sizeof(*out));

  while (isspace(buffer[ret])) {
    ++ret;
    if (ret >= buf_sz) return 0;
  }

  // TODO: parse arrays and objects
  if (isdigit(buffer[ret])) {
    out->type = JSON_TYPE_NUMBER;
    int tmp = 0;
    if (sscanf(&buffer[ret], "%lf%n", &out->v.as_number, &tmp) != 1) return ret;
    ret += tmp;
    return ret;
  }

  switch (buffer[ret]) {
    case 'n':
      if (buf_sz < 4) break;
      if (memcmp(buffer, "null", 4)) break;
      ret += 4;
      out->type = JSON_TYPE_NULL;
      break;

    case 't':
      if (buf_sz < 4) break;
      if (memcmp(buffer, "true", 4)) break;
      ret += 4;
      out->type = JSON_TYPE_BOOLEAN;
      out->v.as_boolean = JSON_TRUE;
      break;

    case 'f':
      if (buf_sz < 5) break;
      if (memcmp(buffer, "false", 5)) break;
      ret += 5;
      out->type = JSON_TYPE_BOOLEAN;
      out->v.as_boolean = JSON_FALSE;
      break;

    case '"': {
      out->type = JSON_TYPE_STRING;
      out->v.as_string.is_heap = JSON_TRUE;
      size_t nalloced = 0;

      size_t str_idx = 0;
      size_t i;
      for (i = ret + 1; i < buf_sz; ++i) {
        if (buffer[i] == '"') break;
  
        if (str_idx + 4 > nalloced) {
          nalloced = (nalloced)? 2 * nalloced : 64;
          out->v.as_string.data = (char *)realloc(out->v.as_string.data, nalloced);
          if (!out->v.as_string.data) break; // TODO: error handling
        }

        if (buffer[i] == '\\') {
          ++i;
          switch (buffer[i]) {
            case '"':  out->v.as_string.data[str_idx++] = '"';  continue;
            case '\\': out->v.as_string.data[str_idx++] = '\\'; continue;
            case '/':  out->v.as_string.data[str_idx++] = '/';  continue;
            case 'b':  out->v.as_string.data[str_idx++] = '\b'; continue;
            case 'f':  out->v.as_string.data[str_idx++] = '\f'; continue;
            case 'n':  out->v.as_string.data[str_idx++] = '\n'; continue;
            case 'r':  out->v.as_string.data[str_idx++] = '\r'; continue;
            case 't':  out->v.as_string.data[str_idx++] = '\t'; continue;
            case 'u': {
              int n = 0, m = 0;
              unsigned short hi = 0, lo = 0;
              if (sscanf(&buffer[i], "u%4hx%n", &hi, &n) != 1) {
                printf("here? %x\n", hi);
                memcpy(&out->v.as_string.data[str_idx], "\ufffd", 3);
                str_idx += 3;
                i += n + m;
                continue;
              }

              if (hi >= 0xd800 && hi <= 0xdbff) {
                if (sscanf(&buffer[i+n], "\\u%4hx%n", &lo, &m) != 1 || lo < 0xdc00 || lo > 0xdfff) {
                  printf("here? %d\n", __LINE__);
                  memcpy(&out->v.as_string.data[str_idx], "\ufffd", 3);
                  str_idx += 3;
                  i += n + m;
                  continue;
                }

                const unsigned long codepoint = 0x10000l + (((hi - 0xd800) << 10l) | (lo - 0xdc00));
                out->v.as_string.data[str_idx++] = 0xf0 | ((codepoint >> 18) & 7);
                out->v.as_string.data[str_idx++] = 0x80 | ((codepoint >> 12) & 0x3f);
                out->v.as_string.data[str_idx++] = 0x80 | ((codepoint >> 6)  & 0x3f);
                out->v.as_string.data[str_idx++] = 0x80 | (codepoint & 0x3f);
                i += n + m - 1;
                continue;
              }

              if (hi <= 0x7F) {
                out->v.as_string.data[str_idx++] = (char)hi;
                i += n + m;
                continue;
              }

              if (hi <= 0x7FF) {
                out->v.as_string.data[str_idx++] = (char)(0xc0 | ((hi >> 6) & 0x1f));
                out->v.as_string.data[str_idx++] = (char)(0x80 | (hi & 0x3f));
                i += n + m;
                continue;
              }

              out->v.as_string.data[str_idx++] = (char)(0xe0 | ((hi >> 12) & 0xf));
              out->v.as_string.data[str_idx++] = (char)(0x80 | ((hi >> 6) & 0x3f));
              out->v.as_string.data[str_idx++] = (char)(0x80 | (hi & 0x3f));
              i += n + m;
            }
            continue;
            default: break; // TODO: error handling
          }
        }

        if ((unsigned char)buffer[i] <= 0x7f) {
          out->v.as_string.data[str_idx++] = buffer[i];
          continue;
        }

        if ((unsigned char)buffer[i] >> 5 == 6) {
          printf("2: %.2s\n", &buffer[i]);
          out->v.as_string.data[str_idx++] = buffer[i++];
          out->v.as_string.data[str_idx++] = buffer[i];
          continue;
        }

        if ((unsigned char)buffer[i] >> 4 == 0xe) {
          printf("3: %.3s\n", &buffer[i]);
          out->v.as_string.data[str_idx++] = buffer[i++];
          out->v.as_string.data[str_idx++] = buffer[i++];
          out->v.as_string.data[str_idx++] = buffer[i];
          continue;
        }
  
        printf("4: %.4s\n", &buffer[i]);
        out->v.as_string.data[str_idx++] = buffer[i++];
        out->v.as_string.data[str_idx++] = buffer[i++];
        out->v.as_string.data[str_idx++] = buffer[i++];
        out->v.as_string.data[str_idx++] = buffer[i];
        continue;
      }

      out->v.as_string.len = str_idx;
      ret = i + 1;
    } break;

    case '[': {
      out->type = JSON_TYPE_ARRAY;
      memset(&out->v.as_array, 0, sizeof(Json_Array));

      ret += 1;
      do {
        while (isspace(buffer[ret]) && ret < buf_sz) ++ret;
        Json_Value elem;
        ret += Json_parseStr(buf_sz - ret, &buffer[ret], &elem);
        Json_arrayAppend(out, &elem);
      } while (buffer[ret++] == ',');
      
      while (isspace(buffer[ret]) && ret < buf_sz) ++ret;
      if (buffer[ret] != ']') break;
    } break;

    case '{': {
      out->type = JSON_TYPE_OBJECT;
      memset(&out->v.as_object, 0, sizeof(Json_Object));

      ret += 1;
      do {
        while (isspace(buffer[ret]) && ret < buf_sz) ++ret;
        Json_Value name, val;
        ret += Json_parseStr(buf_sz - ret, &buffer[ret], &name) ;
        if (name.type != JSON_TYPE_STRING) break;
        while (isspace(buffer[ret]) && ret < buf_sz) ++ret;
        if (buffer[ret++] != ':') break;
        while (isspace(buffer[ret]) && ret < buf_sz) ++ret;
        ret += Json_parseStr(buf_sz - ret, &buffer[ret], &val);
        Json_objectSet(out, name.v.as_string, &val);
      } while (buffer[ret++] == ',');
      
      while (isspace(buffer[ret]) && ret < buf_sz) ++ret;
      if (buffer[ret] != '}') break;
    } break;

  }

  while (isspace(buffer[ret]) && ret < buf_sz) ++ret;
  return ret;
}

#endif // JSON_IMPLEMENTATION
