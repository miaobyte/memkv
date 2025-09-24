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
typedef enum
{
    VT_AUTO = 0,
    VT_STRING,
    VT_I64,
    VT_I32,
    VT_U64,
    VT_U32,
    VT_U8,
    VT_BOOL
} val_type_t;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <pool_path> <cmd> [args] [type flags]\n"
            "pool_path may be a tmpfs/shm file (e.g. /dev/shm/kvpool) or a regular file on disk.\n"
            "If the file does not exist, create it and set its size using: truncate -s <size> <pool_path>\n"
            "Commands:\n"
            "  init <size>[K|M|G] [ratios]   create new pool file (ratios default 3:1:2)\n"
            "  set  <key> <value>   [type]    store value\n"
            "  get  <key>           [type]    fetch value\n"
            "  del  <key>                     delete key\n"
            "  keys [prefix]                  list keys (optionally under prefix)\n"
            "Type flags (choose one for set/get):\n"
            "  -i64 -i32 -u64 -u32 -u8 -s -b\n"
            "Notes:\n"
            "  1) For a new pool file you can create and set size: truncate -s 4M /dev/shm/kvpool\n"
            "  2) Strings are stored with terminating NUL.\n",
            prog);
}
static size_t parse_size_arg(const char *s)
{
    if (!s || !*s)
        return 0;
    char *end;
    unsigned long long base = strtoull(s, &end, 10);
    if (end && *end)
    {
        if (*end == 'K' || *end == 'k')
            base <<= 10;
        else if (*end == 'M' || *end == 'm')
            base <<= 20;
        else if (*end == 'G' || *end == 'g')
            base <<= 30;
        else
            return 0;
        if (end[1] != '\0')
            return 0;
    }
    return (size_t)base;
}

static val_type_t parse_type_flag(const char *arg)
{
    if (!arg)
        return VT_AUTO;
    if (strcmp(arg, "-i64") == 0)
        return VT_I64;
    if (strcmp(arg, "-i32") == 0)
        return VT_I32;
    if (strcmp(arg, "-u64") == 0)
        return VT_U64;
    if (strcmp(arg, "-u32") == 0)
        return VT_U32;
    if (strcmp(arg, "-u8") == 0)
        return VT_U8;
    if (strcmp(arg, "-s") == 0)
        return VT_STRING;
    if (strcmp(arg, "-b") == 0)
        return VT_BOOL;
    return VT_AUTO;
}

static void print_value(void *valptr, val_type_t t)
{
    if (!valptr)
    {
        printf("(null)\n");
        return;
    }
    switch (t)
    {
    case VT_I64:
    {
        int64_t v;
        memcpy(&v, valptr, sizeof(v));
        printf("%lld\n", (long long)v);
        break;
    }
    case VT_I32:
    {
        int32_t v;
        memcpy(&v, valptr, sizeof(v));
        printf("%d\n", v);
        break;
    }
    case VT_U64:
    {
        uint64_t v;
        memcpy(&v, valptr, sizeof(v));
        printf("%llu\n", (unsigned long long)v);
        break;
    }
    case VT_U32:
    {
        uint32_t v;
        memcpy(&v, valptr, sizeof(v));
        printf("%u\n", v);
        break;
    }
    case VT_U8:
    {
        uint8_t v;
        memcpy(&v, valptr, sizeof(v));
        printf("%u\n", (unsigned)v);
        break;
    }
    case VT_BOOL:
    {
        uint8_t v;
        memcpy(&v, valptr, sizeof(v));
        printf("%s\n", v ? "true" : "false");
        break;
    }
    case VT_STRING:
    case VT_AUTO:
    default:
        printf("%s\n", (char *)valptr);
        break;
    }
}

/* decode + print each key */
static void keys_cb(const void *key_data, size_t key_len)
{
    char buf[1024];
    if (key_len >= sizeof(buf))
        return;
    int r = miaobyte_decode((const uint8_t *)key_data, buf, key_len);
    if (r != 0)
    {
        printf("<invalid-key>\n");
        return;
    }
    printf("%s\n", buf);
}

static void check_meta(void *pool, size_t filesize)
{
    if (!pool)
        return;
    memkv_meta_t *meta = (memkv_meta_t *)pool;

    const int width = 50;
    for (int i = 0; i < width; ++i)
        putchar('-');
    putchar('\n');
    printf("memkv meta:\n");
    printf(" %-22s : '%.*s'\n", "magic", 6, meta->magic);
    printf(" %-22s : %u\n", "char_type", (unsigned)meta->char_type);
    printf(" %-22s : %lu\n", "pool_size", (unsigned long)meta->pool_size);
    if (meta->pool_size != filesize)
    {
        printf(" [WARNING] pool_size in meta (%lu) does not match actual file size (%lu)\n",
               (unsigned long)meta->pool_size, (unsigned long)filesize);
    }
    printf(" %-22s : %lu\n", "key_offset", (unsigned long)meta->key_offset);
    printf(" %-22s : %lu\n", "valueptr_offset", (unsigned long)meta->valueptr_offset);
    printf(" %-22s : %lu\n", "value_offset", (unsigned long)meta->value_offset);

    for (int i = 0; i < width; ++i)
        putchar('-');
    putchar('\n');
}
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage(argv[0]);
        return 1;
    }
    const char *pool_path = argv[1];
    const char *cmd = argv[2];
    if (strcmp(cmd, "init") == 0)
    {
        if (argc < 4)
        {
            fprintf(stderr, "init requires size\n");
            return 1;
        }
        if (access(pool_path, F_OK) == 0)
        {
            fprintf(stderr, "file exists: %s\n", pool_path);
            return 1;
        }
        size_t sz = parse_size_arg(argv[3]);
        if (sz == 0)
        {
            fprintf(stderr, "invalid size: %s\n", argv[3]);
            return 1;
        }

        uint8_t keymem = 3, valueptrmem = 1, valuemem = 2;
        if (argc >= 5)
        {
            unsigned a, b, c;
            if (sscanf(argv[4], "%u:%u:%u", &a, &b, &c) == 3 && a && b && c)
            {
                keymem = (uint8_t)a;
                valueptrmem = (uint8_t)b;
                valuemem = (uint8_t)c;
            }
            else
            {
                fprintf(stderr, "invalid ratios (expect a:b:c), using default 3:1:2\n");
            }
        }

        int fd = open(pool_path, O_CREAT | O_EXCL | O_RDWR, 0644);
        if (fd < 0)
        {
            perror("open");
            return 1;
        }
        if (ftruncate(fd, (off_t)sz) != 0)
        {
            perror("ftruncate");
            close(fd);
            unlink(pool_path);
            return 1;
        }

        void *pool = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (pool == MAP_FAILED)
        {
            perror("mmap");
            close(fd);
            unlink(pool_path);
            return 1;
        }

        int r = miaobyte_init(pool, sz, keymem, valueptrmem, valuemem);
        if (r != MEMKV_SUCCESS)
        {
            fprintf(stderr, "init failed: %s\n", memkv_strerror(r));
            munmap(pool, sz);
            close(fd);
            unlink(pool_path);
            return 1;
        }
        printf("initialized pool '%s' size=%zu ratios=%u:%u:%u char_type=48\n",
               pool_path, sz, keymem, valueptrmem, valuemem);
        munmap(pool, sz);
        close(fd);
        return 0;
    }
    /* open existing pool file */
    int fd = open(pool_path, O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }
    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        perror("fstat");
        close(fd);
        return 1;
    }
    size_t pool_size = (size_t)st.st_size;
    if (pool_size == 0)
    {
        fprintf(stderr, "pool file size is 0\n");
        close(fd);
        return 1;
    }

    void *pool = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pool == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return 1;
    }

    // perform meta check before executing any command
    check_meta(pool, pool_size);

    int retcode = 0;

    if (strcmp(cmd, "set") == 0)
    {
        if (argc < 5)
        {
            usage(argv[0]);
            retcode = 1;
            goto done;
        }
        const char *key = argv[3];
        const char *valstr = argv[4];
        val_type_t t = VT_AUTO;
        if (argc >= 6)
            t = parse_type_flag(argv[5]);
        const void *buf = NULL;
        size_t len = 0;
        int64_t i64;
        int32_t i32;
        uint64_t u64;
        uint32_t u32;
        uint8_t u8;
        switch (t)
        {
        case VT_I64:
            i64 = strtoll(valstr, NULL, 0);
            buf = &i64;
            len = sizeof(i64);
            break;
        case VT_I32:
            i32 = (int32_t)strtol(valstr, NULL, 0);
            buf = &i32;
            len = sizeof(i32);
            break;
        case VT_U64:
            u64 = strtoull(valstr, NULL, 0);
            buf = &u64;
            len = sizeof(u64);
            break;
        case VT_U32:
            u32 = (uint32_t)strtoul(valstr, NULL, 0);
            buf = &u32;
            len = sizeof(u32);
            break;
        case VT_U8:
            u8 = (uint8_t)strtoul(valstr, NULL, 0);
            buf = &u8;
            len = sizeof(u8);
            break;
        case VT_BOOL:
            u8 = (strcmp(valstr, "true") == 0 || strcmp(valstr, "1") == 0) ? 1 : 0;
            buf = &u8;
            len = 1;
            break;
        case VT_STRING:
        case VT_AUTO:
        default:
            buf = valstr;
            len = strlen(valstr) + 1;
            break;
        }
        int r = miaobyte_set(pool, key, strlen(key), buf, len);
        if (r != MEMKV_SUCCESS)
        {
            fprintf(stderr, "set failed: %s\n", memkv_strerror(r));
            retcode = 1;
        }
    }
    else if (strcmp(cmd, "get") == 0)
    {
        if (argc < 4)
        {
            usage(argv[0]);
            retcode = 1;
            goto done;
        }
        const char *key = argv[3];
        val_type_t t = VT_AUTO;
        if (argc >= 5)
            t = parse_type_flag(argv[4]);
        void *v = miaobyte_get(pool, key, strlen(key));
        if (!v)
        {
            fprintf(stderr, "not found\n");
            retcode = 1;
        }
        else
        {
            print_value(v, t);
        }
    }
    else if (strcmp(cmd, "del") == 0)
    {
        if (argc < 4)
        {
            usage(argv[0]);
            retcode = 1;
            goto done;
        }
        const char *key = argv[3];
        int r = miaobyte_del(pool, key, strlen(key));
        if (r != MEMKV_SUCCESS)
        {
            fprintf(stderr, "del failed: %s\n", memkv_strerror(r));
            retcode = 1;
        }
    }
    else if (strcmp(cmd, "keys") == 0)
    {
        const char *prefix = NULL;
        size_t plen = 0;
        if (argc >= 4)
        {
            prefix = argv[3];
            plen = strlen(prefix);
        }
        if (!prefix)
        {
            prefix = "";
            plen = 0;
        }
        miaobyte_keys(pool, prefix, plen, keys_cb);
    }
    else
    {
        fprintf(stderr, "unknown cmd: %s\n", cmd);
        usage(argv[0]);
        retcode = 1;
    }

done:
    munmap(pool, pool_size);
    close(fd);
    return retcode;
}
