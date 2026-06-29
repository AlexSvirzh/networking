#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <ftw.h>
#include <getopt.h>
#include <sys/stat.h>

uint8_t *search_bytes = NULL;
size_t search_len = 0;
bool debug_mode = false;

void print_help(const char *prog_name) {
    printf("Использование: %s [опции] <каталог> <цель_поиска>\n", prog_name);
    printf("Опции:\n");
    printf("  -h, --help     Вывод справки по опциям.\n");
    printf("  -v, --version  Вывод версии программы и информации об авторе.\n");
    printf("\nФормат цели_поиска: 0xhh[hh*] (например, 0xc0ffee)\n");
}

void print_version(void) {
    printf("Версия 1.0\nВыполнил: Свиржевский А.Д.\nГруппа: N3252\nВариант: 3 (ftw)\n");
}

bool parse_hex_string(const char *hex_str) {
    if (strncmp(hex_str, "0x", 2) != 0 && strncmp(hex_str, "0X", 2) != 0) {
        fprintf(stderr, "Ошибка: аргумент цель_поиска должен начинаться с префикса 0x.\n");
        return false;
    }
    hex_str += 2;
    size_t len = strlen(hex_str);
    if (len == 0 || len % 2 != 0) {
        fprintf(stderr, "Ошибка: после 0x должно быть четное количество шестнадцатеричных цифр.\n");
        return false;
    }
    search_len = len / 2;
    search_bytes = (uint8_t*)malloc(search_len*sizeof(uint8_t));
    if (!search_bytes) {
        perror("Ошибка выделения памяти (malloc)");
        return false;
    }
    for (size_t i = 0; i < search_len; i++) {
        char byte_str[3] = {hex_str[2*i], hex_str[2*i + 1], '\0' };
        char *endptr;
        long val = strtol(byte_str, &endptr, 16);

        if (*endptr != '\0' || val < 0 || val > 255) {
            fprintf(stderr, "Ошибка: недопустимый шестнадцатеричный байт '%s'.\n", byte_str);
            free(search_bytes);
            search_bytes = NULL;
            return false;
        }
        search_bytes[i] = (uint8_t)val;
    }
    return true;
}

bool search_in_file(const char *fpath) {
    FILE *f = fopen(fpath, "rb"); // Открываем в бинарном режиме
    if (!f) {
        // Ошибка доступа к файлу — выводим сообщение в поток ошибок и продолжаем работу
        fprintf(stderr, "Ошибка доступа к файлу '%s': %s\n", fpath, strerror(errno));
        return false;
    }

    int c;
    size_t match_idx = 0;
    long match_offset = -1;
    long current_offset = 0;

    // Читаем файл побайтово
    while ((c = fgetc(f)) != EOF) {
        if ((uint8_t)c == search_bytes[match_idx]) {
            if (match_idx == 0) {
                match_offset = current_offset; // Запоминаем начало совпадения
            }
            match_idx++;
            
            if (match_idx == search_len) {
                fclose(f);
                if (debug_mode) {
                    fprintf(stderr, "[LAB1DEBUG] Файл '%s' соответствует критерию. "
                                    "Совпадение найдено по смещению: 0x%lX\n", fpath, match_offset);
                }
                return true;
            }
        } else {
            if (match_idx > 0) {
                // Если совпадение сорвалось посередине, нужно вернуться назад.
                // Пример: ищем 0xAA 0xAA 0xBB в файле "0xAA 0xAA 0xAA 0xBB".
                if (fseek(f, match_offset + 1, SEEK_SET) != 0) {
                    fprintf(stderr, "Ошибка позиционирования в файле '%s': %s\n", fpath, strerror(errno));
                    fclose(f);
                    return false; 
                }
                current_offset = match_offset;
                match_idx = 0;
            }
        }
        current_offset++;
    }
    if (ferror(f)) {
        fprintf(stderr, "Ошибка чтения файла '%s': %s\n", fpath, strerror(errno));
        clearerr(f);
    }

    fclose(f);
    return false;
}

int ftw_callback(const char *fpath, const struct stat *sb, int typeflag) {
    (void)sb; // Подавляем предупреждение о неиспользуемом параметре
    if (typeflag == FTW_DNR) {
        fprintf(stderr, "Ошибка доступа к директории '%s': %s\n", fpath, strerror(errno));
        return 0; // Продолжать обход
    }
    if (typeflag == FTW_NS) {
        fprintf(stderr, "Ошибка получения информации о файле '%s': %s\n", fpath, strerror(errno));
        return 0; // Продолжать обход
    }
    if (typeflag == FTW_F) {
        if (search_in_file(fpath)) {
            printf("%s\n", fpath);
        }
    }
    return 0; // Продолжать обход
}

int main(int argc, char *argv[]) {
    if (getenv("LAB1DEBUG") != NULL) {
        debug_mode = true;
    }

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'v':
                print_version(); // Вызываем вывод версии
                return 0;
            case 'h':
                print_help(argv[0]); // Вызываем вывод справки
                return 0;
            default:
                fprintf(stderr, "Неверная опция. Используйте --help для справки.\n");
                return 1;
        }
    }
    if (optind + 2 != argc) {
        fprintf(stderr, "Ошибка: неверное количество аргументов.\n");
        print_help(argv[0]);
        return 1;
    }

    const char *start_dir = argv[optind];
    const char *hex_str = argv[optind + 1];
    if (!parse_hex_string(hex_str)) {
        return 1; 
    }

    if (debug_mode) {
        fprintf(stderr, "[LAB1DEBUG] Запуск поиска в каталоге: %s\n", start_dir);
        fprintf(stderr, "[LAB1DEBUG] Искомая последовательность байтов: ");
        for (size_t i = 0; i < search_len; i++) {
            fprintf(stderr, "%02X ", search_bytes[i]);
        }
        fprintf(stderr, "\n");
    }

    int nopenfd = 20;
    if (ftw(start_dir, ftw_callback, nopenfd) == -1) {
        fprintf(stderr, "Фатальная ошибка при обходе дерева каталогов (ftw): %s\n", strerror(errno));
        free(search_bytes);
        return 1;
    }

    free(search_bytes);
    return 0;
}
