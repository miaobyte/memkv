#ifndef MIAOBYTE_H
#define MIAOBYTE_H

#include <stdint.h>
#include <string.h>

#include <memkv/memkv.h>

int miaobyte_init(void *pool_data,const size_t pool_len,uint8_t keymem,uint8_t valueptrmem,uint8_t valuemem);
int miaobyte_set(void *pool_data, const void *key_data, size_t key_len, const void *value_data, size_t value_len);
void* miaobyte_get(void *pool_data, const void *key_data, size_t key_len);
int miaobyte_del(void *pool_data, const void *key_data, size_t key_len);
void miaobyte_keys(void *pool_data, const void *prefix_data, size_t prefix_len, void (*func)(const void *key_data, size_t key_len));

int miaobyte_encode(const char *str, uint8_t *bytes, size_t len) ;
int miaobyte_decode(const uint8_t *bytes, char *str, size_t len) ;

#endif // MIAOBYTE_H