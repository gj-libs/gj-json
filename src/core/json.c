#include "gj_json/gj_json.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define VALIDATE_AT(ps, p, ret) \
    if (p >= ps->size) \
        return ret;

#define VALIDATE(ps, n, ret) \
    if ((ps->i + n) >= ps->size) \
        return ret;

#define ADVANCE(ps, n, ret) \
    if ((ps->i + n) >= ps->size) \
        return ret; \
    ps->i += n;

#define READ(ps) \
    ps->s[ps->i]

struct parse_state {
    const char *s;
    size_t size;
    size_t i;
};

int parse_object(struct parse_state *ps, struct json_object *obj);
struct json_value parse_value(struct parse_state *ps);

void gj_print_json_value(struct json_value *jv, const char *buffer, int indent) {
    if (jv->json_type == TYPE_INVALID) {
        printf("Invalid JSON value\n");
        return;
    }
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    if (jv->json_type == TYPE_STRING) {
        if (jv->key.key.length > 0) {
            printf("%.*s: ", (int)jv->key.key.length, &buffer[jv->key.key.start]);
        }
        printf("%.*s\n",
                (int)jv->data.string.length, &buffer[jv->data.string.start]);
    } else if (jv->json_type == TYPE_INT) {
        if (jv->key.key.length > 0) {
            printf("%.*s: ", (int)jv->key.key.length, &buffer[jv->key.key.start]);
        }
        printf("%d\n",
                jv->data.integer.value);
    } else if (jv->json_type == TYPE_FLOAT) {
        if (jv->key.key.length > 0) {
            printf("%.*s: ", (int)jv->key.key.length, &buffer[jv->key.key.start]);
        }
        printf("%f\n",
                jv->data.flt.value);
    } else if (jv->json_type == TYPE_BOOL) {
        if (jv->key.key.length > 0) {
            printf("%.*s: ", (int)jv->key.key.length, &buffer[jv->key.key.start]);
        }
        printf("%s\n",
                jv->data.boolean.value ? "true" : "false");
    } else if (jv->json_type == TYPE_NULL) {
        if (jv->key.key.length > 0) {
            printf("%.*s: ", (int)jv->key.key.length, &buffer[jv->key.key.start]);
        }
        printf("null\n");
    } else if (jv->json_type == TYPE_OBJECT) {
        if (jv->key.key.length > 0) {
            printf("%.*s: ", (int)jv->key.key.length, &buffer[jv->key.key.start]);
        }
        printf("{\n");
        gj_print_json_object(&jv->data.object, buffer, indent + 1);
        for (int i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("}\n");
    } else if (jv->json_type == TYPE_ARRAY) {
        if (jv->key.key.length > 0) {
            printf("%.*s: ", (int)jv->key.key.length, &buffer[jv->key.key.start]);
        }
        printf("[\n");
        for (int i = 0; i < jv->data.array.num_elements; i++) {
            gj_print_json_value(&jv->data.array.elements[i], buffer, indent + 1);
        }
        for (int i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("]\n");
    }
}

void gj_print_json_object(struct json_object *obj, const char *buffer, int indent) {
    for (int i = 0; i < obj->num_kv_pairs; i++) {
        gj_print_json_value(&obj->kv_pairs[i], buffer, indent);
    }
}

void gj_print_json(struct JSON *json) {
    gj_print_json_object(&json->root, json->buffer, 0);
}

void skip_whitespaces(struct parse_state *ps) {
    char c = READ(ps);
    while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ADVANCE(ps, 1, (void)0);
        c = READ(ps);
    }
}

struct json_value parse_string(struct parse_state *ps) {
    struct json_value jv = {.json_type = TYPE_INVALID};
    if (READ(ps) != '"')
        return jv;

    ADVANCE(ps, 1, jv); // consume opening quote
    size_t start = ps->i;
    size_t len = 0;

    char c = READ(ps); 
    while (c != '"') {
        ADVANCE(ps, 1, jv);
        c = READ(ps);
    }

    len = ps->i - start;

    ADVANCE(ps, 1, jv); // consume closing quote

    jv.json_type = TYPE_STRING;
    jv.data.string.start = start;
    jv.data.string.length = len;

    return jv;
}

struct json_value parse_number(struct parse_state *ps) {
    // TODO: check the length of the number to be less than 32/64 bits
    struct json_value jv = {.json_type = TYPE_INVALID};

    size_t start = ps->i;
    char c = READ(ps);

    if (c == '-') {
        ADVANCE(ps, 1, jv);
        c = READ(ps);
    }

    while (c >= '0' && c <= '9') {
        ADVANCE(ps, 1, jv);
        c = READ(ps);
    }

    if (c == '.') {
        jv.json_type = TYPE_FLOAT;

        ADVANCE(ps, 1, jv);
        c = READ(ps);

        while (c >= '0' && c <= '9') {
            ADVANCE(ps, 1, jv);
            c = READ(ps);
        }

        jv.data.flt.value = strtod(&ps->s[start], NULL);
    } else {
        jv.json_type = TYPE_INT;

        int value = 0;
        int multiplier = 1;

        VALIDATE_AT(ps, start, jv);
        if (ps->s[start] == '-') {
            multiplier = -1;
            start++;
        }

        VALIDATE_AT(ps, ps->i, jv);
        for (size_t i = start; i < ps->i; i++) {
            value = value * 10 + (ps->s[i] - '0');
        }

        jv.data.integer.value = value * multiplier;
    }

    skip_whitespaces(ps);
    c = READ(ps);

    if (c == ',' || c == '}' || c == ']')
        return jv;

    return (struct json_value){.json_type = TYPE_INVALID};
}

struct json_value parse_bool_null(struct parse_state *ps) {
    struct json_value jv = {.json_type = TYPE_INVALID};
    // TODO: check overflow for strncmp
    if (strncmp(&READ(ps), "true", 4) == 0) {
        jv.json_type = TYPE_BOOL;
        jv.data.boolean.value = 1;
        ADVANCE(ps, 4, (struct json_value){.json_type = TYPE_INVALID})
    } else if (strncmp(&READ(ps), "false", 5) == 0) {
        jv.json_type = TYPE_BOOL;
        jv.data.boolean.value = 0;
        ADVANCE(ps, 5, (struct json_value){.json_type = TYPE_INVALID})
    } else if (strncmp(&READ(ps), "null", 4) == 0) {
        jv.json_type = TYPE_NULL;
        jv.data.null.value = 0;
        ADVANCE(ps, 4, (struct json_value){.json_type = TYPE_INVALID})
    }
    return jv;
}

int parse_array(struct parse_state *ps, struct json_array *arr) {
    if (READ(ps) != '[')
        return -1;

    ADVANCE(ps, 1, -1);

    char c = READ(ps); 
    while (c != ']') {
        skip_whitespaces(ps);
        struct json_value jv = parse_value(ps);
        arr->num_elements++;
        arr->elements = realloc(arr->elements, arr->num_elements * sizeof(struct json_value));
        arr->elements[arr->num_elements - 1] = jv;
        skip_whitespaces(ps);
        c = READ(ps); 
        if (c == ',') {
            ADVANCE(ps, 1, -1);
            c = READ(ps); 
        } else if (c != ']') {
            return -1;
        }
    }

    ADVANCE(ps, 1, -1);

    return 0;
}

struct json_value parse_value(struct parse_state *ps) {
    struct json_value jv = {.json_type = TYPE_INVALID};
    char c = READ(ps);
    if (c == '"') {
        return parse_string(ps);
    } else if ((c >= '0' && c <= '9') || c == '-') {
        return parse_number(ps);
    } else if (c == 't' || c == 'f' || c == 'n') {
        return parse_bool_null(ps);
    } else if (c == '{') {
        jv.json_type = TYPE_OBJECT;
        jv.data.object.num_kv_pairs = 0;
        jv.data.object.kv_pairs = NULL;
        if (parse_object(ps, &jv.data.object))
            return jv;
    } else if (c == '[') {
        jv.json_type = TYPE_ARRAY;
        jv.data.array.num_elements = 0;
        jv.data.array.elements = NULL;
        parse_array(ps, &jv.data.array);
    }
    return jv;
}

int parse_kv_pair(struct parse_state *ps, struct json_object *obj) {
    skip_whitespaces(ps);

    struct json_value jv = parse_string(ps);
    if (jv.json_type != TYPE_STRING)
        return -1;
    struct json_key jk = {.key = {.start = jv.data.string.start, .length = jv.data.string.length}};

    skip_whitespaces(ps);

    if (READ(ps) != ':')
        return -1;

    ADVANCE(ps, 1, -1);

    skip_whitespaces(ps);
    struct json_value jv2 = parse_value(ps);
    jv2.key = jk;

    obj->num_kv_pairs++;
    obj->kv_pairs = realloc(obj->kv_pairs, obj->num_kv_pairs * sizeof(struct json_value));
    obj->kv_pairs[obj->num_kv_pairs - 1] = jv2;
    return 0;
}

int parse_object(struct parse_state *ps, struct json_object *obj) {
    skip_whitespaces(ps);
    if (READ(ps) != '{')
        return -1;
    ADVANCE(ps, 1, -1); // consume opening brace
    while (READ(ps) != '}') {
        if (parse_kv_pair(ps, obj))
            return -1;
        skip_whitespaces(ps);
        if (READ(ps) == ',') {
            ADVANCE(ps, 1, -1);
            skip_whitespaces(ps);
        }
    }
    ADVANCE(ps, 1, -1); // consume closing brace
    return 0;
}

void gj_json_free(struct JSON *json) {
    for (int i = 0; i < json->root.num_kv_pairs; i++) {
        struct json_value *jv = &json->root.kv_pairs[i];
        if (jv->json_type == TYPE_OBJECT) {
            struct JSON nested_json = {jv->data.object, NULL};
            gj_json_free(&nested_json);
        } else if (jv->json_type == TYPE_ARRAY) {
            for (int j = 0; j < jv->data.array.num_elements; j++) {
                struct json_value *elem = &jv->data.array.elements[j];
                if (elem->json_type == TYPE_OBJECT) {
                    struct JSON nested_json = {elem->data.object, NULL};
                    gj_json_free(&nested_json);
                }
            }
            free(jv->data.array.elements);
        }
    }
    free(json->root.kv_pairs);
    if (json->buffer) {
        free(json->buffer);
        json->buffer = NULL;
    }
}

struct json_value gj_json_get(struct JSON *json, const char *key) {
    for (int i = 0; i < json->root.num_kv_pairs; i++) {
        struct json_value *jv = &json->root.kv_pairs[i];
        if (jv->key.key.length == strlen(key) &&
                strncmp(&json->buffer[jv->key.key.start], key, jv->key.key.length) == 0) {
            return *jv;
        }
        if (jv->json_type == TYPE_OBJECT) {
            struct JSON nested_json = {jv->data.object, json->buffer};
            struct json_value nested_jv = gj_json_get(&nested_json, key);
            if (nested_jv.json_type != TYPE_INVALID) {
                return nested_jv;
            }
        }
    }
    return (struct json_value){.json_type = TYPE_INVALID};
}

int gj_json_parse(const char *filename, struct JSON *json) {
    FILE *fptr;

    if (!(fptr = fopen(filename, "rb"))) {
        return -1;
    }

    int fd = fileno(fptr);
    struct stat buf;
    fstat(fd, &buf);
    off_t size = buf.st_size;

    char *buffer = malloc(size);

    if (fread(buffer, size, 1, fptr) != 1) {
        free(buffer);
        fclose(fptr);
        return -1;
    }

    struct parse_state ps = {buffer, size, 0};
    VALIDATE((&ps), 1, -1);
    struct json_object obj = {};
    if (parse_object(&ps, &obj)) {
        free(buffer);
        fclose(fptr);
        return -1;
    }
    fclose(fptr);
    *json = (struct JSON){obj, buffer};
    return 0;
}
