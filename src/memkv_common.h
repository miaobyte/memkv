#ifndef MEMKV_COMMON_H
#define MEMKV_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include <box_malloc/box_malloc.h>
#include <block_malloc/block_malloc.h>


typedef struct
{   
    #define MEMKV_MAGIC "memkv"
    uint8_t magic[6]; // "memkv"
    uint16_t char_type; // 字符类别数量，不超过256，因为unicode可以用更多的byte(uint8_t)表示
    uint64_t pool_size; // 内存池大小
    // 三块区域的偏移
    uint64_t key_offset;
    uint64_t valueptr_offset;
    uint64_t value_offset;

    // key区
    blocks_meta_t keys_blocks;
}  memkv_meta_t;

typedef struct{
    bool has_key:1;
    uint64_t box_offset:63;//如果has_key=1,表示该节点存储了一个key,box_offset表示key对应的对象偏移
    int32_t child_key_blocks[2];//实际不为2，而是=char_type。
}  __attribute__((packed))  key_node_t;


#endif // MEMKV_COMMON_H