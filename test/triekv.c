#include <stdio.h>
#include <string.h>

#include "stdutil/triekv.h"
#include "stdutil/logutil.h"
#include "stdutil/block.h"


#define POOL_SIZE (1 << 20) // 1MB内存池

void test_meta(bytes_t pool) {
     // 初始化内存池
    triekv_setmeta(pool.data, pool.len,37);
}

void key2embed(const bytes_t key,const bytes_t mapped_key) {
    if (!key.data || !mapped_key.data) return;

    for (size_t i = 0; i < key.len; i++) {
        char c = key.data[i];

        // 映射规则：a-z -> 0-25, 0-9 -> 26-35, '_' -> 36
        if (c >= 'a' && c <= 'z') {
            mapped_key.data[i] = c - 'a'; // 映射到 0-25
        } else if (c >= '0' && c <= '9') {
            mapped_key.data[i] = c - '0' + 26; // 映射到 26-35
        } else if (c == '_') {
            mapped_key.data[i] = 36; // 映射到 36
        } else {
            mapped_key.data[i] = 0xFF; // 非法字符映射为 0xFF
        }
    }
}
void embed2key(const bytes_t mapped_key, const bytes_t original_key) {
    if (!mapped_key.data || !original_key.data) return;

    for (size_t i = 0; i < mapped_key.len; i++) {
        uint8_t mapped_char = mapped_key.data[i];

        // 逆向映射规则：0-25 -> a-z, 26-35 -> 0-9, 36 -> '_', 其他 -> '?'
        if (mapped_char <= 25) {
            original_key.data[i] = 'a' + mapped_char; // 映射回 a-z
        } else if (mapped_char >= 26 && mapped_char <= 35) {
            original_key.data[i] = '0' + (mapped_char - 26); // 映射回 0-9
        } else if (mapped_char == 36) {
            original_key.data[i] = '_'; // 映射回 '_'
        } else {
            original_key.data[i] = '?'; // 非法字符映射为 '?'
        }
    }
}

void* print_key(const bytes_t mapped_key) {
    BYTES_BUFFER(src_key,mapped_key.len+1);
    embed2key(mapped_key, src_key);
    src_key.data[mapped_key.len]='\0';
    LOG("Key: %s", (char*)src_key.data);
    return NULL;
}

void test_set(bytes_t mem_pool) {
    bytes_t key =BYTES_LITERAL("example_key");
    BYTES_BUFFER(mapped_key,key.len);
    key2embed(key, mapped_key); // 更新键的映射
    triekv_set(mem_pool, mapped_key, 1024);

    bytes_t key2 =BYTES_LITERAL("eabcdefg");
    BYTES_BUFFER(mapped_key2,key2.len);
    key2embed(key2, mapped_key2); // 更新键的映射
    triekv_set(mem_pool, mapped_key2, 2048);


    bytes_t value=BYTES_LITERAL("example_value");
}

void test_get(bytes_t mem_pool) {
    bytes_t key =BYTES_LITERAL("example_key");
    BYTES_BUFFER(mapped_key,key.len);
    key2embed(key, mapped_key); // 更新键的映射


    bytes_t key2 =BYTES_LITERAL("eabcdefg");
    BYTES_BUFFER(mapped_key2,key2.len);
    key2embed(key2, mapped_key2); // 更新键的映射

}

int main() {
    LOG("Starting triekv test");
    BYTES_BUFFER(pool, POOL_SIZE);
    test_meta(pool);
    test_set(pool);

    bytes_t prefix =BYTES_LITERAL("e");
    BYTES_BUFFER(mapped_prefix,prefix.len);
    key2embed(prefix, mapped_prefix); // 更新前缀的映射
    triekv_keys(pool,mapped_prefix,print_key);

    test_get(pool);
    return 0;
}