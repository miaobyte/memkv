#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <memkv/memkv.h>
#include "memkv_common.h"
#include "logutil.h"

static size_t keynode_size(const memkv_meta_t *meta)
{
    return  sizeof(uint64_t)+ sizeof(int32_t)*meta->char_type;
}
static int keynode_init(const memkv_meta_t *meta,void *nodeptr)
{
    if (!nodeptr)
    {
        LOG("[ERROR] keynode_init node is NULL");
        return -1;
    }
    key_node_t *node = (key_node_t *)nodeptr;
    node->has_key = false;
    node->box_offset = 0;
    for (size_t i = 0; i < meta->char_type; i++)
    {
        node->child_key_blocks[i] = -1; // 初始化所有子节点=-1
    }
    return 0;
}

static size_t align_to_power_of_16_times_8(size_t size) {
    if (size < 8) return 0;
    size_t base = size / 8;
    int n = 0;
    while (base >= 16) {
        base /= 16;
        n++;
    }
    size_t alignment = 8 * (1ULL << (4 * n)); // 16^n = 2^(4n)
    return (size / alignment) * alignment;
}

int memkv_init(void *pool_data,const size_t pool_len,  const uint16_t chartype,uint8_t keymem,uint8_t valueptrmem,uint8_t valuemem)
{
    //memkv meta区
    if (!pool_data||pool_len<=0)
    {
        LOG("[ERROR] pool is NULL");
        return MEMKV_ERROR_INVALID_ARG;
    }
    if (pool_len <= sizeof(memkv_meta_t))
    {
        LOG("[ERROR] pool size %lu is too small", pool_len);
        return MEMKV_ERROR_OUTOFMEMORY;
    }
    memkv_meta_t *meta = (memkv_meta_t *)pool_data;
    if (memcmp(meta->magic, MEMKV_MAGIC, sizeof(meta->magic)) == 0)
    {
        LOG("[INFO] memkv already initialized");
        return MEMKV_ERROR_ALREADY_INIT; // 已经初始化
    }
    memcpy(meta->magic, MEMKV_MAGIC, sizeof(meta->magic));
    meta->pool_size = pool_len;
    meta->char_type = chartype;

    LOG("[INFO] meta size: %zu", sizeof(memkv_meta_t));

    //分割剩余的pool，为key,valueptr,value三块
    size_t total_available = pool_len - sizeof(memkv_meta_t);

    // 根据比例计算各部分大小（未对齐）
    size_t total_proportion = keymem + valueptrmem + valuemem;
    size_t keys_size_raw = (total_available * keymem) / total_proportion;
    size_t valueptr_size_raw = (total_available * valueptrmem) / total_proportion;
    size_t value_size_raw = (total_available * valuemem) / total_proportion;

    // 8字节对齐（向下取整）
    size_t keys_size = (keys_size_raw / 8) * 8;
    size_t valueptr_size = (valueptr_size_raw / 8) * 8;
    size_t value_size = align_to_power_of_16_times_8(value_size_raw);

    // 调整最后一个部分以使用剩余空间（确保总和 <= total_available）
    size_t used_size = keys_size + valueptr_size + value_size;
    if (used_size > total_available) {
        value_size -= (used_size - total_available);
        // 再次对齐
        value_size = align_to_power_of_16_times_8(value_size);
    }

    LOG("[INFO] keys_size: %zu, valueptr_size: %zu, value_size: %zu", keys_size, valueptr_size, value_size);
    meta->key_offset = sizeof(memkv_meta_t);
    meta->valueptr_offset = meta->key_offset + keys_size;
    meta->value_offset = meta->valueptr_offset + valueptr_size;

    //key 区
    size_t keynodesize = keynode_size(meta);
    blocks_init(&(meta->keys_blocks), keys_size, keynodesize);
    void *keys_start = pool_data + meta->key_offset;
    blocks_alloc(&meta->keys_blocks,keys_start); // 分配根节点
    void *root_key =keys_start+ blockdata_offset(&meta->keys_blocks, 0);
    if (!root_key)
    {
        LOG("[ERROR] failed to allocate root node");
        return -1;
    }
    keynode_init(meta, root_key); // 初始化根节点

    //valueptr和values区
    void *boxptr_start = pool_data + meta->valueptr_offset;
    int result =box_init(boxptr_start, valueptr_size, value_size);
    if (result < 0)
    {
        LOG("[ERROR] box_init failed");
        return -1;
    }
    LOG("[INFO] memkv root node initialized");
    return MEMKV_SUCCESS;
}

int memkv_set(void *pool_data, const void *key_data, size_t key_len, const void *value_data, size_t value_len)
{
    if (!pool_data  || !key_data || key_len < 0)
        return MEMKV_ERROR_INVALID_ARG;

    memkv_meta_t *meta = (memkv_meta_t *)pool_data;

    void* key_start = pool_data + meta->key_offset;
    key_node_t *root_node = key_start + blockdata_offset(&meta->keys_blocks, 0);
    key_node_t *cur_node = root_node;

    for (size_t i = 0; i < key_len; i++)
    {
        // 当前字符的索引
        uint8_t char_index = *(uint8_t *)(key_data + i);
        
        int32_t childi = cur_node->child_key_blocks[char_index];
        if (childi < 0)
        {
            // 如果没有子节点，分配一个新的节点
            int64_t new_block_id = blocks_alloc(&meta->keys_blocks, key_start);
            if (new_block_id<0)
            {
                LOG("[ERROR] failed to allocate new block for char %c at depth %zu", char_index, i);
                return MEMKV_ERROR_OUTOFMEMORY;
            }
            key_node_t *parent_node = cur_node; 
            parent_node->child_key_blocks[char_index] = new_block_id;
            cur_node = key_start + blockdata_offset(&meta->keys_blocks, new_block_id);
            keynode_init(meta, cur_node); // 初始化新节点             // 更新当前节点的子节点指针
        }
        else
        {
            cur_node = key_start + blockdata_offset(&meta->keys_blocks, childi); // 跳转到子节点
        }
    }

    // 设置当前节点的hasobj_offset为value
    
    void *valueptr_start = pool_data + meta->valueptr_offset;
    void *value_start = pool_data + meta->value_offset;
    if (cur_node->has_key)
    {
        LOG("[INFO] key already exists, deleting value");
        box_free(valueptr_start, cur_node->box_offset); // 释放旧的对象
    }
    uint64_t newobj_offset= box_alloc(valueptr_start, value_len); // 分配新的对象
    if (newobj_offset == (uint64_t)-1)
    {
        LOG("[ERROR] box_alloc failed for value of size %zu", value_len);
        return MEMKV_ERROR_OUTOFMEMORY;
    }
    cur_node->box_offset = newobj_offset; // 更新实际的对象偏移
    cur_node->has_key = true;
    memcpy(value_start + newobj_offset, value_data, value_len); // 复制新值
    LOG("[INFO] key set successfully,objoffset %lu", newobj_offset);
    return MEMKV_SUCCESS;
}
 
 
void* memkv_get(void *pool_data, const void *key_data, size_t key_len)
{
    if (!pool_data || !key_data || key_len <= 0)
    {
        LOG("[ERROR] invalid arguments to memkv_get");
        return NULL;
    }

    memkv_meta_t *meta = (memkv_meta_t *)pool_data;
    void* key_start = pool_data + meta->key_offset;
    key_node_t *root_node = key_start + blockdata_offset(&meta->keys_blocks, 0);
    key_node_t *cur_node = root_node;

    // 沿着前缀树遍历
    for (size_t i = 0; i < key_len; i++)
    {
        uint8_t char_index = ((uint8_t*)key_data)[i];
        int32_t childi = cur_node->child_key_blocks[char_index];
        if (childi < 0)
        {
            // 未找到子节点，表示键不存在
            LOG("[INFO] key not found at character %zu", i);
            return NULL;
        }
        // 跳转到子节点
        cur_node = key_start + blockdata_offset(&meta->keys_blocks, childi);
    }

    // 到达最后一个节点，检查是否有值
    if (!cur_node->has_key)
    {
        LOG("[INFO] key path found but no key set");
        return NULL;
    }

    // 获取value的指针和值
    void *value_start = pool_data + meta->value_offset;
    uint64_t value_offset = cur_node->box_offset;
    
    // 构造返回值
    void* result= value_start + value_offset;
    LOG("[INFO] key found");
    return result;
}
int memkv_del(void* pool_data, const void* key_data, size_t key_len)
{
    if (!pool_data || !key_data || key_len <= 0)
    {
        LOG("[ERROR] invalid arguments to memkv_del");
        return MEMKV_ERROR_INVALID_ARG;
    }

    memkv_meta_t *meta = (memkv_meta_t *)pool_data;
    void* key_start = pool_data + meta->key_offset;
    key_node_t *root_node = key_start + blockdata_offset(&meta->keys_blocks, 0);
    key_node_t *cur_node = root_node;

    // 沿着前缀树遍历
    for (size_t i = 0; i < key_len; i++)
    {
        uint8_t char_index = ((uint8_t*)key_data)[i];
        int32_t childi = cur_node->child_key_blocks[char_index];
        if (childi < 0)
        {
            // 未找到子节点，表示键不存在
            LOG("[INFO] key not found at character %zu, nothing to delete", i);
            return MEMKV_ERROR_KEY_NOT_FOUND;
        }
        // 跳转到子节点
        cur_node = key_start + blockdata_offset(&meta->keys_blocks, childi);
    }

    // 到达最后一个节点，检查是否有值
    if (!cur_node->has_key)
    {
        LOG("[INFO] key found but has no value, nothing to delete");
        return MEMKV_ERROR_KEY_NOT_FOUND;
    }

    // 释放与键关联的值
    void *valueptr_start = pool_data + meta->valueptr_offset;
    box_free(valueptr_start, cur_node->box_offset);
    
    // 将节点标记为没有值
    cur_node->has_key = false;
    cur_node->box_offset = 0;
    
    LOG("[INFO] key and associated value deleted successfully");
    return MEMKV_SUCCESS;
}
#define KEY_BUFFER_MAX 1024
static void memkv_traverse_dfs(memkv_meta_t *meta, key_node_t *node, char *key_buffer, size_t depth, void (*func)(const void* key_data, size_t key_len))
{
    if (!node)
        return;

    // 如果当前节点有值，调用回调函数处理键
    if (node->has_key)
    {
        func(key_buffer, depth);
    }

    // 遍历所有子节点
    for (size_t i = 0; i < meta->char_type; i++)
    {
        int32_t child_id = node->child_key_blocks[i];
        if (child_id >= 0)
        {
            void *key_start = (uint8_t *)meta + meta->key_offset;
            key_node_t *child_node = key_start + blockdata_offset(&meta->keys_blocks, child_id);
            
            if (depth + 1 >= KEY_BUFFER_MAX) {
                LOG("[ERROR] would overflow key buffer at depth %zu, skipping child %zu", depth+1, i);
                continue;
            }
            
            key_buffer[depth] = (char)i; // 将当前字符加入键
            memkv_traverse_dfs(meta, child_node, key_buffer, depth + 1, func);
        }
    }
}

void memkv_keys(void* pool_data, const void* prefix_data,size_t prefix_len, void (*func)(const void* key_data, size_t key_len))
{
    if (!pool_data || !func)
    {
        LOG("[ERROR] invalid arguments to memkv_keys");
        return;
    }

    memkv_meta_t *meta = (memkv_meta_t *)pool_data;
    void *key_start = pool_data + meta->key_offset;
    key_node_t *root_node =  key_start + blockdata_offset(&meta->keys_blocks, 0);
    if (!root_node)
    {
        LOG("[ERROR] root node is NULL");
        return;
    }

    char key_buffer[KEY_BUFFER_MAX];
    size_t depth = 0;

    // 将前缀写入 key_buffer
    if (prefix_data && prefix_len > 0)
    {
        if (prefix_len >= sizeof(key_buffer))
        {
            LOG("[ERROR] prefix is too long");
            return;
        }
        memcpy(key_buffer, prefix_data, prefix_len);
        depth = prefix_len;

        // 遍历到前缀对应的节点
        key_node_t *cur_node = root_node;
        for (size_t i = 0; i < prefix_len; i++)
        {
            uint8_t char_index = ((uint8_t*)prefix_data)[i];
            if (char_index >= meta->char_type)
            {
                LOG("[ERROR] character index out of range: %u", char_index);
                return;
            }
            int32_t child_id = cur_node->child_key_blocks[char_index];
            if (child_id < 0)
            {
                LOG("[INFO] prefix not found");
                return; // 前缀不存在
            }
            cur_node =key_start + blockdata_offset(&meta->keys_blocks, child_id);
        }

        // 从前缀节点开始递归遍历
        memkv_traverse_dfs(meta, cur_node, key_buffer, depth, func);
    }
    else
    {
        // 如果没有前缀，从根节点开始遍历
        memkv_traverse_dfs(meta, root_node, key_buffer, depth, func);
    }
}

const char* memkv_strerror(memkv_error_t err)
{
    switch (err) {
        case MEMKV_SUCCESS:
            return "Success";
        case MEMKV_ERROR_INVALID_ARG:
            return "Invalid argument";
        case MEMKV_ERROR_POOL_NULL:
            return "Memory pool is null";
        case MEMKV_ERROR_OUTOFMEMORY:
            return "Memory pool out of memory";
        case MEMKV_ERROR_ALREADY_INIT:
            return "Already initialized";
        case MEMKV_ERROR_ALLOC_FAILED:
            return "Allocation failed";
        case MEMKV_ERROR_KEY_NOT_FOUND:
            return "Key not found";
        case MEMKV_ERROR_KEY_EXISTS:
            return "Key already exists";
        case MEMKV_ERROR_PREFIX_TOO_LONG:
            return "Prefix is too long";
        case MEMKV_ERROR_CHAR_OUT_OF_RANGE:
            return "Character index out of range";
        case MEMKV_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}