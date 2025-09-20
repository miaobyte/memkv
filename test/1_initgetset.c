#define POOL_SIZE (1 << 20) // 1MB内存池
#include <string.h>
#include <stdint.h>

#include <memkv/memkv.h>
#include "logutil.h"

int main(){
    uint8_t pool[POOL_SIZE];
    memkv_init(pool, POOL_SIZE, 256, 1, 1, 2);

    memkv_set(pool, "hello", 5, "world", 5);
    char* value = (char*)memkv_get(pool, "hello", 5);
    if (value) {
        LOG("Value for 'hello': %s", value);
    } else {
        LOG("Key 'hello' not found");
    }
    return 0;
}