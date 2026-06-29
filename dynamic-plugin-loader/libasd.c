#define _XOPEN_SOURCE 700 
#include <stdio.h>        
#include <stdlib.h>       
#include <string.h>       
#include <errno.h>        
#include <arpa/inet.h>    
#include <stdbool.h>      
#include "plugin_api.h"   

static struct plugin_option my_options[] = {
    {
        // name: "ipv4-addr-bin", has_arg: required_argument (обязателен), flag: 0, val: 0
        {"ipv4-addr-bin", required_argument, 0, 0},
        "Поиск файлов, содержащих заданное значение IPv4-адреса в бинарной форме..." 
    }
};

// Хост вызывает это, передает пустую структуру ppi, а мы её заполняем
int plugin_get_info(struct plugin_info* ppi) {
    if (!ppi) return -1; 
    ppi->plugin_purpose = "Поиск IPv4 адресов в бинарном виде";
    ppi->plugin_author = "Свиржевский А.Д., N3252";
    ppi->sup_opts_len = 1;      
    ppi->sup_opts = my_options; 
    return 0; 
}

// Хост передает нам имя файла и опции, которые юзер ввел в консоли для НАС
int plugin_process_file(const char *fname, struct option in_opts[], size_t in_opts_len) {
    char *ip_str = NULL; 
    bool debug = (getenv("LAB2DEBUG") != NULL);

    for (size_t i = 0; i < in_opts_len; i++) {
        if (strcmp(in_opts[i].name, "ipv4-addr-bin") == 0) {
            ip_str = (char *)in_opts[i].flag; 
            break;
        }
    }

    if (!ip_str) {
        return 1; 
    }

    uint32_t ip_be;
    if (inet_pton(AF_INET, ip_str, &ip_be) != 1) {
        if (debug) fprintf(stderr, "[libasdN3252] Ошибка: некорректный IPv4 адрес '%s'\n", ip_str);
        errno = EINVAL; 
        return -1;
    }

    uint32_t ip_le = __builtin_bswap32(ip_be);

    FILE *f = fopen(fname, "rb");
    if (!f) {
        if (debug) fprintf(stderr, "[libasdN3252] Ошибка открытия '%s': %s\n", fname, strerror(errno));
        return -1; 
    }

    uint8_t buf[4096]; 
    size_t bytes_read; 
    off_t offset = 0;  

    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        
        for (size_t i = 0; i + 3 < bytes_read; i++) {
            if (memcmp(buf + i, &ip_be, 4) == 0 || memcmp(buf + i, &ip_le, 4) == 0) {
                if (debug) {
                    fprintf(stderr, "[libasdN3252] Найдено совпадение в '%s' по смещению 0x%lX\n", 
                            fname, (long)(offset + i));
                }
                fclose(f);
                return 0; 
            }
        }

        if (bytes_read == sizeof(buf)) {
            fseek(f, -3, SEEK_CUR); 
            offset += bytes_read - 3; 
        } else {
            offset += bytes_read; 
        }
    }

    if (ferror(f)) {
        if (debug) fprintf(stderr, "[libasdN3252] Ошибка чтения '%s': %s\n", fname, strerror(errno));
        fclose(f);
        return -1; 
    }

    fclose(f);
    return 1; 
}
