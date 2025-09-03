#ifndef  MEMKV_H
#define MEMKV_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t len;
} bytes_t;

int memkv_init(const void *pool, const uint64_t pool_size, const uint16_t chartype);
void memkv_set(const void *pool,const bytes_t key,const uint64_t valueptr);
bytes_t memkv_get(const bytes_t pool,const bytes_t key);
void memkv_keys(const bytes_t pool,const bytes_t prefix,void (*func)(const bytes_t key));
void memkv_del(const bytes_t pool,const bytes_t key);

#endif // MEMKV_H