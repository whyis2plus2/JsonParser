#include <stdio.h>

#define JSON_IMPLEMENTATION 1
#include "../json.h"

int main(void)
{
  Json_Value object;

  FILE *file = fopen("test.json", "r");
  if (!file) return 1;
  Json_parseFile(file, &object);
  fclose(file);

  Json_objectDelete(&object, JSON_STRLIT("foo 🌿"));
  Json_objectSetNum(&object, JSON_STRLIT("field2"), -INFINITY);
  Json_objectSetStr(&object, JSON_STRLIT("field"), JSON_STRLIT("qwerty 🌳"));

  Json_printValue(stdout, &object);
  Json_destroyValue(&object);
  return 0;
}