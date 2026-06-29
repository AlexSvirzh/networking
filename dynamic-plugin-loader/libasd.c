#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include "plugin_api.h"

static struct plugin_option my_options[] = {
    {
        {"bit-seq", required_argument, 0, 0},
        "Поиск файлов по битовой последовательности (поддерживаются 0b, 0x и десятичный формат)"
    }
};

int plugin_get_info(struct plugin_info* ppi) {
    if (!ppi) return -1;
    ppi->plugin_purpose = "Поиск произвольной битовой последовательности (Variant 5)";
    ppi->plugin_author = "Ахмедов Фазл, N3251";
    ppi->sup_opts_len = 1;
    ppi->sup_opts = my_options;
    return 0;
}

// Умный парсер строки в битовый массив
bool parse_bit_seq(const char *str, uint8_t **pat_data, size_t *pat_bits) {
    if (strncmp(str, "0b", 2) == 0 || strncmp(str, "0B", 2) == 0) {
        str += 2;
        size_t len = strlen(str);
        if (len == 0) return false;
        *pat_bits = len;
        *pat_data = calloc(1, (len + 7) / 8);
        for (size_t i = 0; i < len; i++) {
            if (str[i] == '1') {
                (*pat_data)[i / 8] |= (1 << (7 - (i % 8)));
            } else if (str[i] != '0') {
                free(*pat_data); return false;
            }
        }
        return true;
    } 
    else if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        str += 2;
        size_t len = strlen(str);
        if (len == 0) return false;
        *pat_bits = len * 4;
        *pat_data = calloc(1, (len * 4 + 7) / 8);
        for (size_t i = 0; i < len; i++) {
            char c = tolower(str[i]);
            int val = 0;
            if (c >= '0' && c <= '9') val = c - '0';
            else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
            else { free(*pat_data); return false; }
            
            for (int b = 0; b < 4; b++) {
                if ((val >> (3 - b)) & 1) {
                    size_t bit_idx = i * 4 + b;
                    (*pat_data)[bit_idx / 8] |= (1 << (7 - (bit_idx % 8)));
                }
            }
        }
        return true;
    } 
    else {
        char *endptr;
        unsigned long long val = strtoull(str, &endptr, 10);
        if (*endptr != '\0' || strlen(str) == 0) return false;
        
        size_t bits = 0;
        unsigned long long temp = val;
        if (temp == 0) bits = 1;
        else {
            while (temp > 0) { bits++; temp >>= 1; }
        }
        
        *pat_bits = bits;
        *pat_data = calloc(1, (bits + 7) / 8);
        for (size_t i = 0; i < bits; i++) {
            if ((val >> (bits - 1 - i)) & 1) {
                (*pat_data)[i / 8] |= (1 << (7 - (i % 8)));
            }
        }
        return true;
    }
}

int plugin_process_file(const char *fname, struct option in_opts[], size_t in_opts_len) {
    const char *seq_str = NULL;
    bool debug = (getenv("LAB2DEBUG") != NULL);

    for (size_t i = 0; i < in_opts_len; i++) {
        if (strcmp(in_opts[i].name, "bit-seq") == 0) {
            seq_str = (char*)in_opts[i].flag;
            break;
        }
    }
    if (!seq_str) return 1;

    uint8_t *pat_data = NULL;
    size_t pat_bits = 0;
    if (!parse_bit_seq(seq_str, &pat_data, &pat_bits)) {
        if (debug) fprintf(stderr, "[libafN3251] Ошибка парсинга bit-seq: %s\n", seq_str);
        errno = EINVAL;
        return -1;
    }

    FILE *f = fopen(fname, "rb");
    if (!f) {
        if (debug) fprintf(stderr, "[libafN3251] Ошибка доступа к '%s': %s\n", fname, strerror(errno));
        free(pat_data);
        return -1; 
    }

    uint8_t buf[8192];
    size_t overlap_bytes = (pat_bits + 7) / 8;
    
    // Защита от гигантских битовых паттернов, превышающих буфер
    if (overlap_bytes > sizeof(buf) / 2) {
        fclose(f); free(pat_data);
        return -1;
    }

    size_t bytes_in_buf = 0;
    bool found = false;
    
    // Блочное чтение для поиска паттерна на границе блоков
    while (!feof(f) && !ferror(f)) {
        size_t to_read = sizeof(buf) - bytes_in_buf;
        size_t n = fread(buf + bytes_in_buf, 1, to_read, f);
        bytes_in_buf += n;

        if (bytes_in_buf == 0) break;

        size_t bits_in_buf = bytes_in_buf * 8;
        if (bits_in_buf >= pat_bits) {
            size_t search_limit = bits_in_buf - pat_bits;
            
            // Проверка совпадения на любом битовом смещении
            for (size_t i = 0; i <= search_limit; i++) {
                bool match = true;
                for (size_t j = 0; j < pat_bits; j++) {
                    size_t buf_bit_idx = i + j;
                    uint8_t buf_bit = (buf[buf_bit_idx / 8] >> (7 - (buf_bit_idx % 8))) & 1;
                    uint8_t pat_bit = (pat_data[j / 8] >> (7 - (j % 8))) & 1;
                    if (buf_bit != pat_bit) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    found = true;
                    if (debug) fprintf(stderr, "[libafN3251] Найдено совпадение в %s!\n", fname);
                    break;
                }
            }
        }
        
        if (found) break;

        // Оставляем конец буфера для склейки со следующим куском файла
        if (bytes_in_buf >= overlap_bytes) {
            memmove(buf, buf + bytes_in_buf - overlap_bytes, overlap_bytes);
            bytes_in_buf = overlap_bytes;
        } else {
            break;
        }
    }

    int ret = found ? 0 : 1;
    if (ferror(f)) ret = -1;
    
    fclose(f);
    free(pat_data);
    return ret;
}
