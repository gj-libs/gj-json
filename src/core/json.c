#include "gj_json/gj_json.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define INVALID \
    (struct json_value){.json_type = TYPE_INVALID}

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
    if (READ(ps) != '"')
        return INVALID;

    ADVANCE(ps, 1, INVALID); // consume opening quote
    size_t start = ps->i;
    size_t len = 0;

    char c = READ(ps); 
    while (c != '"') {
        ADVANCE(ps, 1, INVALID);
        c = READ(ps);
    }

    len = ps->i - start;

    ADVANCE(ps, 1, INVALID); // consume closing quote

    struct json_value jv = {
        .json_type = TYPE_STRING,
        .data.string.start = start,
        .data.string.length = len
    };

    return jv;
}

struct json_value parse_number(struct parse_state *ps) {
    // TODO: check the length of the number to be less than 32/64 bits
    size_t start = ps->i;
    char c = READ(ps);

    int multiplier = 1;

    if (c == '-') {
        ADVANCE(ps, 1, INVALID);
        c = READ(ps);
        multiplier = -1;
    }

    while (c >= '0' && c <= '9') {
        ADVANCE(ps, 1, INVALID);
        c = READ(ps);
    }

    struct json_value jv = INVALID;
    if (c == '.') {
        jv.json_type = TYPE_FLOAT;

        ADVANCE(ps, 1, INVALID);
        c = READ(ps);

        while (c >= '0' && c <= '9') {
            ADVANCE(ps, 1, INVALID);
            c = READ(ps);
        }

        jv.data.flt.value = strtod(&ps->s[start], NULL);
    } else {
        jv.json_type = TYPE_INT;

        int value = 0;
        if (multiplier == -1)
            start++;
        VALIDATE_AT(ps, ps->i, INVALID);
        for (size_t i = start; i < ps->i; i++) {
            value = value * 10 + (ps->s[i] - '0');
        }

        jv.data.integer.value = value * multiplier;
    }

    skip_whitespaces(ps);
    c = READ(ps);

    if (c == ',' || c == '}' || c == ']')
        return jv;

    return INVALID;
}

struct json_value parse_bool_null(struct parse_state *ps) {
    struct json_value jv = INVALID;
    // TODO: check overflow for strncmp
    if (strncmp(&READ(ps), "true", 4) == 0) {
        jv.json_type = TYPE_BOOL;
        jv.data.boolean.value = 1;
        ADVANCE(ps, 4, INVALID);
    } else if (strncmp(&READ(ps), "false", 5) == 0) {
        jv.json_type = TYPE_BOOL;
        jv.data.boolean.value = 0;
        ADVANCE(ps, 5, INVALID);
    } else if (strncmp(&READ(ps), "null", 4) == 0) {
        jv.json_type = TYPE_NULL;
        jv.data.null.value = 0;
        ADVANCE(ps, 4, INVALID);
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
    struct json_value jv = INVALID;
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
            return INVALID;
    } else if (c == '[') {
        jv.json_type = TYPE_ARRAY;
        jv.data.array.num_elements = 0;
        jv.data.array.elements = NULL;
        if (parse_array(ps, &jv.data.array))
            return INVALID;
    }
    return jv;
}

int parse_kv_pair(struct parse_state *ps, struct json_object *obj) {
    skip_whitespaces(ps);

    struct json_value jv = parse_string(ps);
    if (jv.json_type != TYPE_STRING)
        return -1;
    struct json_key jk = {
        .key = {
            .start = jv.data.string.start,
            .length = jv.data.string.length
        }
    };

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

int parse_index(struct parse_state *ps, int *num) {
    char c = READ(ps);

    if (c != '[')
        return -1;
    ADVANCE(ps, 1, -1);

    size_t start = ps->i;
    int multiplier = 1;
    c = READ(ps);
    if (c == '-') {
        ADVANCE(ps, 1, -1);
        c = READ(ps);
        start++;
        multiplier = -1;
    }

    while (c >= '0' && c <= '9') {
        ADVANCE(ps, 1, -1);
        c = READ(ps);
    }

    if (c != ']')
        return -1;

    *num = 0;

    VALIDATE_AT(ps, ps->i-1, -1);
    for (size_t i = start; i <= ps->i-1; i++) {
        *num = *num * 10 + (ps->s[i] - '0');
    }
    *num *= multiplier;

    return 0;
}

struct json_value *parse_keys(
        const char *key,
        int *num_keys) {
    if (!key)
        return NULL;

    struct parse_state _ps = {
        .size = strlen(key),
        .i = 0,
        .s = key
    };
    struct parse_state *ps = &_ps;
    struct json_value *keys;
    *num_keys = 0;

    while (1) {
        // valid
        // key.key1[0]
        // key.key1[0].key2
        //
        // invalid
        // .key
        // key.
        // key..key1
        // key.[0]

        if (*num_keys == 0) {
            (*num_keys)++;
            keys = malloc(sizeof(struct json_value) * *num_keys);
            keys[*num_keys - 1].data.string.start = 0;
            keys[*num_keys - 1].data.string.length = 0;
        } else {
            (*num_keys)++;
            keys = realloc(keys, sizeof(struct json_value) * *num_keys);
            keys[*num_keys - 1].data.string.start = 0;
            keys[*num_keys - 1].data.string.length = 0;
        }

        char c = ps->s[ps->i]; 
        if (c == '[') {
            int idx;
            if (parse_index(ps, &idx)) {
                free(keys);
                return NULL;
            }
            keys[*num_keys - 1].json_type = TYPE_INT;
            keys[*num_keys - 1].data.integer.value = idx;
            ps->i++;
            if (ps->i >= ps->size) {
                return keys;
            }
        } else {
            if (c == '.') {
                ps->i++;
                if (ps->i >= ps->size || ps->s[ps->i] == '.' || ps->s[ps->i] == '[') {
                    free(keys);
                    return NULL;
                }
            }
            int start = ps->i;
            int new_key = 0;
            while (ps->i < ps->size) {
                ps->i++;
                c = ps->s[ps->i];
                if (c == '.' || c == '[') {
                    new_key = 1;
                    break;
                }
            }
            keys[*num_keys - 1].json_type = TYPE_STRING;
            keys[*num_keys - 1].data.string.start = start;
            keys[*num_keys - 1].data.string.length = ps->i - start;
            if (!new_key)
                return keys;
        }
    }

    return NULL;
}

struct json_value get_value_by_key(
        struct json_value *root,
        struct json_value *key,
        const char *data_buffer,
        const char *key_buffer) {
    if (root->json_type == TYPE_OBJECT && key->json_type == TYPE_STRING) {
        struct json_object *obj = &root->data.object;
        for (int i = 0; i < obj->num_kv_pairs; i++) {
            struct json_value jv = obj->kv_pairs[i];
            if (jv.key.key.length == key->data.string.length &&
                    strncmp(
                        &data_buffer[jv.key.key.start],
                        &key_buffer[key->data.string.start],
                        jv.key.key.length) == 0) {
                return jv;
            }
        }
    } else if (root->json_type == TYPE_ARRAY && key->json_type == TYPE_INT) {
        int idx = key->data.integer.value;
        if (idx >= root->data.array.num_elements)
            return INVALID;
        if (abs(idx) > root->data.array.num_elements)
            return INVALID;
        return root->data.array.elements[idx];
    }
    return INVALID;
}

struct json_value gj_json_get(struct JSON *json, const char *key) {
    int num_keys;
    struct json_value *keys = parse_keys(key, &num_keys);
    if (!keys)
        return INVALID;
    
    for (int k = 0; k < num_keys; k++) {
        if (keys[k].json_type == TYPE_INT)
            printf("key[%d]: %d\n", k, keys[k].data.integer.value);
        else if (keys[k].json_type == TYPE_STRING)
            printf("key[%d]: %.*s\n", k, (int)keys[k].data.string.length, &key[keys[k].data.string.start]);
    }

    struct json_value root = {.json_type = TYPE_OBJECT, .data = {.object = json->root}};
    for (int k = 0; k < num_keys; k++) {
        struct json_value res = get_value_by_key(&root, &keys[k], json->buffer, key);
        if (res.json_type == TYPE_INVALID)
            return INVALID;
        root = res;
    }

    return root;
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
