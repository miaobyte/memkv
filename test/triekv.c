#include <stdio.h>
#include <string.h>

#include <memkv/memkv.h>
#include "logutil.h"
#include <blockmalloc/blockmalloc.h>


#define POOL_SIZE (1 << 20) // 1MB内存池

void test_meta(void *pool, size_t pool_len) {
     // 初始化内存池
    memkv_init(pool,pool_len, 37, 1, 1, 1);
}

void key2embed(const uint8_t* key,uint8_t* mapped_key, size_t key_len) {
    for (size_t i = 0; i < key_len; i++) {
        char c = key[i];

        // 映射规则：a-z -> 0-25, 0-9 -> 26-35, '_' -> 36
        if (c >= 'a' && c <= 'z') {
            mapped_key[i] = c - 'a'; // 映射到 0-25
        } else if (c >= '0' && c <= '9') {
            mapped_key[i] = c - '0' + 26; // 映射到 26-35
        } else if (c == '_') {
            mapped_key[i] = 36; // 映射到 36
        } else {
            mapped_key[i] = 0xFF; // 非法字符映射为 0xFF
        }
    }
}
void embed2key(const uint8_t* mapped_key, uint8_t* original_key, size_t key_len) {
    if (!mapped_key || !original_key) return;

    for (size_t i = 0; i < key_len; i++) {
        uint8_t mapped_char = mapped_key[i];

        // 逆向映射规则：0-25 -> a-z, 26-35 -> 0-9, 36 -> '_', 其他 -> '?'
        if (mapped_char <= 25) {
            original_key[i] = 'a' + mapped_char; // 映射回 a-z
        } else if (mapped_char >= 26 && mapped_char <= 35) {
            original_key[i] = '0' + (mapped_char - 26); // 映射回 0-9
        } else if (mapped_char == 36) {
            original_key[i] = '_'; // 映射回 '_'
        } else {
            original_key[i] = '?'; // 非法字符映射为 '?'
        }
    }
}

void print_key(const void *mapped_key, size_t key_len) {
    char key[256];
    if (key_len >= sizeof(key)) {
        LOG("Key length %zu exceeds buffer size", key_len);
        return;
    }
    embed2key(mapped_key, key, key_len);
    key[key_len] = '\0';
    LOG("Key: %s", key);
}

void test_set(void *mem_pool_data, size_t mem_pool_len) {
    uint8_t mapped_key[256]; // 假设最大键长度为 256
    const char *key_str = "example_key";
    size_t key_len = strlen(key_str);
    key2embed((const uint8_t*)key_str, mapped_key, key_len);
    const char *value_str = "example_value";
    size_t value_len = strlen(value_str);
    memkv_set(mem_pool_data, mapped_key, key_len, (const void*)value_str, value_len);

    uint8_t mapped_key2[256];
    const char *key2_str = "eabcdefg";
    size_t key2_len = strlen(key2_str);
    key2embed((const uint8_t*)key2_str, mapped_key2, key2_len);
    const char *value2_str = "235346";
    size_t value2_len = strlen(value2_str);
    memkv_set(mem_pool_data, mapped_key2, key2_len, (const void*)value2_str, value2_len);
}

void test_get(void *mem_pool_data, size_t mem_pool_len) {
    uint8_t mapped_key[256];
    const char *key_str = "example_key";
    size_t key_len = strlen(key_str);
    key2embed((const uint8_t*)key_str, mapped_key, key_len);

    uint8_t mapped_key2[256];
    const char *key2_str = "eabcdefg";
    size_t key2_len = strlen(key2_str);
    key2embed((const uint8_t*)key2_str, mapped_key2, key2_len);
}

 

int main() {
    LOG("Starting triekv test");
    uint8_t pool[POOL_SIZE];
    test_set(pool, POOL_SIZE);

    uint8_t mapped_prefix[256];
    const char *prefix_str = "e";
    size_t prefix_len = strlen(prefix_str);
    key2embed((const uint8_t*)prefix_str, mapped_prefix, prefix_len);
    memkv_keys(pool, mapped_prefix, prefix_len, print_key);

    test_get(pool, POOL_SIZE);
    return 0;
}