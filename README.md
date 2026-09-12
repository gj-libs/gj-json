# gj_json

A simple JSON parser.

## Build
```bash
make
```

## Example
```c
#include "gj_json/gj_json.h"

int main() {
    struct JSON json;
    int res = gj_json_parse( "test.json", &json);

    if (res)
        return res;

    // Example queries
    gj_print_json(&json);
    struct json_value val = gj_json_get(&json, "name");

    // Cleanup
    gj_json_free(&json);

    return res;
}
```
