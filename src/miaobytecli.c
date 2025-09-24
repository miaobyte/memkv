#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memkv/miaobyte.h>
#include <memkv/memkv.h>
#include "memkv_common.h"

/* Value type specifiers */
typedef enum {
    VT_AUTO = 0,
    VT_STRING,
    VT_I64,
    VT_I32,
    VT_U64,
    VT_U32,
    VT_U8,
    VT_BOOL
} val_type_t;

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <pool_path> <cmd> [args] [type flags]\n"
        "pool_path may be a tmpfs/shm file (e.g. /dev/shm/kvpool) or a regular file on disk.\n"
        "If the file does not exist, create it and set its size using: truncate -s <size> <pool_path>\n"
        "Commands:\n"
        "  set  <key> <value>   [type]    store value\n"
        "  get  <key>           [type]    fetch value\n"
        "  del  <key>                     delete key\n"
        "  keys [prefix]                  list keys (optionally under prefix)\n"
        "Type flags (choose one for set/get):\n"
        "  -i64 -i32 -u64 -u32 -u8 -s -b\n"
        "Notes:\n"
        "  1) For a new pool file you can create and set size: truncate -s 4M /dev/shm/kvpool\n"
        "  2) Strings are stored with terminating NUL.\n"
        , prog);
}

static val_type_t parse_type_flag(const char *arg) {
    if (!arg) return VT_AUTO;
    if (strcmp(arg, "-i64") == 0) return VT_I64;
    if (strcmp(arg, "-i32") == 0) return VT_I32;
    if (strcmp(arg, "-u64") == 0) return VT_U64;
    if (strcmp(arg, "-u32") == 0) return VT_U32;
    if (strcmp(arg, "-u8")  == 0) return VT_U8;
    if (strcmp(arg, "-s")   == 0) return VT_STRING;
    if (strcmp(arg, "-b")   == 0) return VT_BOOL;
    return VT_AUTO;
}

static void print_value(void *valptr, val_type_t t) {
    if (!valptr) { printf("(null)\n"); return; }
    switch (t) {
        case VT_I64: {
            int64_t v; memcpy(&v, valptr, sizeof(v)); printf("%lld\n", (long long)v); break; }
        case VT_I32: {
            int32_t v; memcpy(&v, valptr, sizeof(v)); printf("%d\n", v); break; }
        case VT_U64: {
            uint64_t v; memcpy(&v, valptr, sizeof(v)); printf("%llu\n", (unsigned long long)v); break; }
        case VT_U32: {
            uint32_t v; memcpy(&v, valptr, sizeof(v)); printf("%u\n", v); break; }
        case VT_U8:  {
            uint8_t v; memcpy(&v, valptr, sizeof(v)); printf("%u\n", (unsigned)v); break; }
        case VT_BOOL:{
            uint8_t v; memcpy(&v, valptr, sizeof(v)); printf("%s\n", v?"true":"false"); break; }
        case VT_STRING:
        case VT_AUTO:
        default:
            printf("%s\n", (char*)valptr);
            break;
    }
}

/* decode + print each key */
static void keys_cb(const void *key_data, size_t key_len) {
    char buf[1024];
    if (key_len >= sizeof(buf)) return;
    miaobyte_decode((const uint8_t*)key_data, buf, key_len);
    buf[key_len] = '\0';
    printf("%s\n", buf);
}

static void check_meta(void *pool, size_t pool_size) {
    if (!pool) return;
    memkv_meta_t *meta = (memkv_meta_t *)pool;

    printf("=== memkv meta check ===\n");
    printf("magic:\t'%.*s'\n", 6, meta->magic);  // 使用 %.*s 打印固定长度字符串
    printf("char_type:\t%u\n", (unsigned)meta->char_type);
    printf("pool_size (meta):\t%lu\n", (unsigned long)meta->pool_size);
    printf("file_mapped_size:\t%lu\n", (unsigned long)pool_size);
    printf("key_offset:\t%lu\n", (unsigned long)meta->key_offset);
    printf("valueptr_offset:\t%lu\n", (unsigned long)meta->valueptr_offset);
    printf("value_offset:\t%lu\n", (unsigned long)meta->value_offset);

    // basic validation
    if (memcmp(meta->magic, MEMKV_MAGIC, 6) != 0) {
        fprintf(stderr, "[WARN] magic mismatch: not a memkv pool or not initialized\n");
    }
    if (meta->key_offset >= pool_size || meta->valueptr_offset >= pool_size || meta->value_offset >= pool_size) {
        fprintf(stderr, "[WARN] offsets outside mapped range\n");
    }
    if (meta->pool_size != 0 && meta->pool_size != pool_size) {
        fprintf(stderr, "[WARN] meta.pool_size (%lu) differs from file size (%lu)\n",
                (unsigned long)meta->pool_size, (unsigned long)pool_size);
    }
    printf("========================\n");
}

int main(int argc, char **argv) {
    if (argc < 3) { usage(argv[0]); return 1; }
    const char *pool_path = argv[1];
    const char *cmd = argv[2];

    /* open existing pool file */
    int fd = open(pool_path, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st; if (fstat(fd, &st) != 0) { perror("fstat"); close(fd); return 1; }
    size_t pool_size = (size_t)st.st_size;
    if (pool_size == 0) { fprintf(stderr, "pool file size is 0\n"); close(fd); return 1; }

    void *pool = mmap(NULL, pool_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (pool == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    // perform meta check before executing any command
    check_meta(pool, pool_size);

    int retcode = 0;

    if (strcmp(cmd, "set") == 0) {
        if (argc < 5) { usage(argv[0]); retcode = 1; goto done; }
        const char *key = argv[3];
        const char *valstr = argv[4];
        val_type_t t = VT_AUTO;
        if (argc >= 6) t = parse_type_flag(argv[5]);
        const void *buf = NULL; size_t len = 0;
        int64_t i64; int32_t i32; uint64_t u64; uint32_t u32; uint8_t u8;
        switch (t) {
            case VT_I64: i64 = strtoll(valstr, NULL, 0); buf=&i64; len=sizeof(i64); break;
            case VT_I32: i32 = (int32_t)strtol(valstr, NULL, 0); buf=&i32; len=sizeof(i32); break;
            case VT_U64: u64 = strtoull(valstr, NULL, 0); buf=&u64; len=sizeof(u64); break;
            case VT_U32: u32 = (uint32_t)strtoul(valstr, NULL, 0); buf=&u32; len=sizeof(u32); break;
            case VT_U8:  u8  = (uint8_t)strtoul(valstr, NULL, 0); buf=&u8; len=sizeof(u8); break;
            case VT_BOOL: u8 = (strcmp(valstr,"true")==0||strcmp(valstr,"1")==0)?1:0; buf=&u8; len=1; break;
            case VT_STRING: case VT_AUTO: default: buf = valstr; len = strlen(valstr)+1; break;
        }
        int r = miaobyte_set(pool, key, strlen(key), buf, len);
        if (r != MEMKV_SUCCESS) { fprintf(stderr, "set failed: %s\n", memkv_strerror(r)); retcode=1; }
    }
    else if (strcmp(cmd, "get") == 0) {
        if (argc < 4) { usage(argv[0]); retcode=1; goto done; }
        const char *key = argv[3];
        val_type_t t = VT_AUTO; if (argc >=5) t = parse_type_flag(argv[4]);
        void *v = miaobyte_get(pool, key, strlen(key));
        if (!v) { fprintf(stderr, "not found\n"); retcode=1; }
        else { print_value(v, t); }
    }
    else if (strcmp(cmd, "del") == 0) {
        if (argc < 4) { usage(argv[0]); retcode=1; goto done; }
        const char *key = argv[3];
        int r = miaobyte_del(pool, key, strlen(key));
        if (r != MEMKV_SUCCESS) { fprintf(stderr, "del failed: %s\n", memkv_strerror(r)); retcode=1; }
    }
    else if (strcmp(cmd, "keys") == 0) {
        const char *prefix = NULL; size_t plen = 0;
        if (argc >=4) { prefix = argv[3]; plen = strlen(prefix); }
        if (!prefix) { prefix=""; plen=0; }
        miaobyte_keys(pool, prefix, plen, keys_cb);
    }
    else {
        fprintf(stderr, "unknown cmd: %s\n", cmd);
        usage(argv[0]);
        retcode=1;
    }

done:
    munmap(pool, pool_size);
    close(fd);
    return retcode;
}
