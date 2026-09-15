/*
 * gj_json
 * Copyright (c) 2026 giji676
 * Licensed under MIT (see LICENSE file)
 */

#ifndef GJ_JSON_H
#define GJ_JSON_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

enum json_type {
    TYPE_INVALID,
    TYPE_STRING,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_NULL,
    TYPE_OBJECT,
    TYPE_ARRAY
};

struct json_int {
    int value;
};

struct json_float {
    float value;
};

struct json_str {
    size_t start;
    size_t length;
};

struct json_bool {
    int value;
};

struct json_null {
    int value;
};

struct json_array {
    int num_elements;
    struct json_value *elements;
};

struct json_value;

struct json_object {
    int num_kv_pairs;
    struct json_value *kv_pairs;
};

union json_data {
    struct json_int integer;
    struct json_float flt;
    struct json_str string;
    struct json_bool boolean;
    struct json_null null;
    struct json_array array;
    struct json_object object;
};

struct json_key {
    struct json_str key;
};

struct json_value {
    int json_type;
    union json_data data;
    struct json_key key;
};

struct JSON {
    struct json_object root;
    char *buffer;
};


/*
 *
 */
int gj_json_parse(const char *filename, struct JSON *json);
void gj_json_free(struct JSON *json);
struct json_value gj_json_get(struct JSON *json, const char *key);
void gj_print_json(struct JSON *json);
void gj_print_json_object(struct json_object *obj, const char *buffer, int indent);

#ifdef __cplusplus
}
#endif

#endif /* GJ_JSON_H */
