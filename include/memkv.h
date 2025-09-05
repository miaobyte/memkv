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

int memkv_init(void *pool_data, size_t pool_len, uint16_t chartype, uint8_t keymem, uint8_t valueptrmem, uint8_t valuemem);
void memkv_set(void *pool_data, const void *key_data, size_t key_len, const void *value_data, size_t value_len);
void* memkv_get(void *pool_data, const void *key_data, size_t key_len);
void memkv_del(void *pool_data, const void *key_data, size_t key_len);
void memkv_keys(void *pool_data, const void *prefix_data, size_t prefix_len, void (*func)(const void *key_data, size_t key_len));

#endif // MEMKV_H