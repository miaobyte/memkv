#define POOL_SIZE (1 << 20) // 1MB内存池
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <memkv/memkv.h>
#include "logutil.h"

int main() {
    uint8_t pool[POOL_SIZE];
    // 初始化memkv，chartype=256（支持ASCII），比例2:1:1
    if (memkv_init(pool, POOL_SIZE, 256, 2, 1, 1) != 0) {
        LOG("[ERROR] memkv_init failed");
        return -1;
    }

    // 测试最大KV容量：循环插入键值对，直到失败
    const char *value = "value"; // 固定值，长度5
    size_t value_len = strlen(value);
    char key[32]; // 键缓冲区
    size_t count = 0;

    while (1) {
        // 生成键，如 "key0", "key1", ...
        snprintf(key, sizeof(key), "key%zu", count);
        size_t key_len = strlen(key);

        // 尝试设置键值对
        memkv_set(pool, key, key_len, value, value_len);

        // 验证是否成功：尝试获取值
        char *retrieved_value = (char *)memkv_get(pool, key, key_len);
        if (retrieved_value && strcmp(retrieved_value, value) == 0) {
            count++; // 成功计数
        } else {
            // 如果获取失败，说明内存不足或设置失败，停止
            LOG("[INFO] Failed to set/get key%zu, stopping", count);
            break;
        }

        // 可选：每1000个打印进度
        if (count % 1000 == 0) {
            LOG("[INFO] Inserted %zu KV pairs", count);
        }
    }

    LOG("[INFO] Maximum KV capacity: %zu pairs", count);

    // 循环结束后，重新检查所有成功的键值对是否存在且未被修改
    LOG("[INFO] Verifying all inserted KV pairs...");
    size_t verified_count = 0;
    for (size_t i = 0; i < count; i++) {
        // 生成键
        snprintf(key, sizeof(key), "key%zu", i);
        size_t key_len = strlen(key);

        // 获取值
        char *retrieved_value = (char *)memkv_get(pool, key, key_len);
        if (retrieved_value && strcmp(retrieved_value, value) == 0) {
            verified_count++;
        } else {
            LOG("[ERROR] Verification failed for key%zu: value mismatch or not found", i);
            break;
        }
    }

    if (verified_count == count) {
        LOG("[INFO] Verification successful: all %zu KV pairs are intact", count);
    } else {
        LOG("[ERROR] Verification failed: only %zu/%zu pairs verified", verified_count, count);
    }

    return 0;
}