#include <memkv/miaobyte.h>
#include <memkv/memkv.h>


/*
key 子符集
a-z  26个
0-9  10个
space 1个
@#_-  /[]  :  ,./ 11个
一共48个字符
*/
static const uint8_t encode_map[256] = {
    // 初始化为 0xFF（未知字符）
    [0 ... 255] = 0xFF,
    // a-z: 0-25
    ['a'] = 0, ['b'] = 1, ['c'] = 2, ['d'] = 3, ['e'] = 4, ['f'] = 5, ['g'] = 6, ['h'] = 7, ['i'] = 8, ['j'] = 9,
    ['k'] = 10, ['l'] = 11, ['m'] = 12, ['n'] = 13, ['o'] = 14, ['p'] = 15, ['q'] = 16, ['r'] = 17, ['s'] = 18, ['t'] = 19,
    ['u'] = 20, ['v'] = 21, ['w'] = 22, ['x'] = 23, ['y'] = 24, ['z'] = 25,
    // 0-9: 26-35
    ['0'] = 26, ['1'] = 27, ['2'] = 28, ['3'] = 29, ['4'] = 30, ['5'] = 31, ['6'] = 32, ['7'] = 33, ['8'] = 34, ['9'] = 35,
    // space: 36
    [' '] = 36,
    // @#_- /[] : ,. : 37-47
    ['@'] = 37, ['#'] = 38, ['_'] = 39, ['-'] = 40, ['/'] = 41, ['['] = 42, [']'] = 43, [':'] = 44, [','] = 45, ['.'] = 46,
};

// 解码查找表：索引为编码字节，值为字符（0-47，共48个）
static const char decode_map[48] = {
    // 0-25: a-z
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
    'u', 'v', 'w', 'x', 'y', 'z',
    // 26-35: 0-9
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    // 36: space
    ' ',
    // 37-46: @#_- /[] : ,.
    '@', '#', '_', '-', '/', '[', ']', ':', ',', '.',
};

void miaobyte_encode(const char *str, uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        bytes[i] = encode_map[(uint8_t)str[i]];
    }
}

void miaobyte_decode(const uint8_t *bytes, char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = bytes[i];
        if (b < 48) {
            str[i] = decode_map[b];
        } else {
            str[i] = '?';
        }
    }
    str[len] = '\0';
}

int miaobyte_init(void *pool_data,const size_t pool_len,uint8_t keymem,uint8_t valueptrmem,uint8_t valuemem){
    return memkv_init(pool_data,pool_len,48,keymem,valueptrmem,valuemem);
}
int miaobyte_set(void *pool_data, const void *key_data, size_t key_len, const void *value_data, size_t value_len){
    uint8_t *encoded_key = malloc(key_len);
    if (!encoded_key)
        return MEMKV_ERROR_OUTOFMEMORY;
    miaobyte_encode((const char*)key_data,encoded_key,key_len);
    int ret= memkv_set(pool_data,encoded_key,key_len,value_data,value_len);
    free(encoded_key);
    return ret;
}

void* miaobyte_get(void *pool_data, const void *key_data, size_t key_len){
    uint8_t *encoded_key = malloc(key_len);
    if (!encoded_key)
        return NULL;
    miaobyte_encode((const char*)key_data,encoded_key,key_len);
    void* ret= memkv_get(pool_data,encoded_key,key_len);
    free(encoded_key);
    return ret;
}
int miaobyte_del(void *pool_data, const void *key_data, size_t key_len){
    uint8_t *encoded_key = malloc(key_len);
    if (!encoded_key)
        return MEMKV_ERROR_OUTOFMEMORY;
    miaobyte_encode((const char*)key_data,encoded_key,key_len);
    int ret= memkv_del(pool_data,encoded_key,key_len);
    free(encoded_key);
    return ret;
}
void miaobyte_keys(void *pool_data, const void *prefix_data, size_t prefix_len, void (*func)(const void *key_data, size_t key_len)){
    uint8_t *encoded_prefix = malloc(prefix_len);
    if (!encoded_prefix)
        return;
    miaobyte_encode((const char*)prefix_data,encoded_prefix,prefix_len);
    memkv_keys(pool_data,encoded_prefix,prefix_len,func);
    free(encoded_prefix);
}
