/*
memkv:是


/*
内存布局示意图：

meta区：
+-------------------+  box_meta_t 描述整个伙伴系统
|   +-------------+ |
|   | buddysize    | |  伙伴系统的总大小
|   | box_size     | |  box区的总大小
|   | blocks_meta_t| |  blocks的元数据
|   +-------------+ |  blocks区，存储 block_t 和 box_head_t
|   | block_t0    | |  每个 block 的元数据
|   | box_head_t[0] | |  描述第0个 box 的状态和结构
|   +-------------+ |
|   | block_t1    | |  每个 block 的元数据
|   | box_head_t[1] | |  描述第1个 box 的状态和结构
|   +-------------+ |
|   | block_t2    | |  每个 block 的元数据
|   | box_head_t[2] | |  描述第2个 box 的状态和结构
|   | ...         | |
|   +-------------+ |
+-------------------+  <-- meta区结束，box区开始

box区：
+-------------------+  <-- 起始地址 (box_root)
|   box 数据区      |  存储实际分配的对象，不包含任何 meta 信息
|                   |
|                   |
+-------------------+  <-- box区结束

说明：
1. meta区和 box区 地址互相独立。
2. meta区存储 box_meta_t 和 blocks 的元数据，用于管理 box 的分配。
3. box区是实际分配的内存区域，不存储任何元数据。
*/

#ifndef  MEMKV_H
#define MEMKV_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MEMKV_SUCCESS = 0,           // 操作成功
    MEMKV_ERROR_INVALID_ARG = -1, // 无效参数
    MEMKV_ERROR_POOL_NULL = -2,   // 内存池为空
    MEMKV_ERROR_OUTOFMEMORY = -3, // 内存池过小
    MEMKV_ERROR_ALREADY_INIT = -4,   // 已初始化
    MEMKV_ERROR_ALLOC_FAILED = -5,   // 分配失败
    MEMKV_ERROR_KEY_NOT_FOUND = -6,  // 键不存在
    MEMKV_ERROR_KEY_EXISTS = -7,     // 键已存在（可选，用于set操作）
    MEMKV_ERROR_PREFIX_TOO_LONG = -8, // 前缀过长
    MEMKV_ERROR_CHAR_OUT_OF_RANGE = -9, // 字符索引超出范围
    MEMKV_ERROR_UNKNOWN = -10         // 未知错误
} memkv_error_t;

int memkv_init(void *pool_data, size_t pool_len, uint16_t chartype, uint8_t keymem, uint8_t valueptrmem, uint8_t valuemem);
int memkv_set(void *pool_data, const void *key_data, size_t key_len, const void *value_data, size_t value_len);
void* memkv_get(void *pool_data, const void *key_data, size_t key_len);
int memkv_del(void *pool_data, const void *key_data, size_t key_len);
void memkv_keys(void *pool_data, const void *prefix_data, size_t prefix_len, void (*func)(const void *key_data, size_t key_len));

// 返回错误码对应的字符串描述
const char* memkv_strerror(memkv_error_t err);

#endif // MEMKV_H