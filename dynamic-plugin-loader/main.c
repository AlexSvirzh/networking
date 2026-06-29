#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ftw.h>
#include <dlfcn.h>
#include <getopt.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include "plugin_api.h"

typedef enum { OP_AND, OP_OR } LogicOp;

typedef struct {
    void *handle;
    char *name;
    struct plugin_info info;
    int (*get_info)(struct plugin_info*);
    int (*process_file)(const char*, struct option[], size_t);
    
    struct option *active_opts;
    size_t active_opts_len;
    size_t active_opts_cap;
} Plugin;

Plugin *plugins = NULL;
size_t plugins_count = 0;

LogicOp global_op = OP_AND; 
bool invert_result = false; 
bool debug_mode = false;

int *opt_to_plugin_map = NULL;

void print_help(const char *prog_name) {
    printf("Использование: %s [опции] [каталог]\n", prog_name);
    printf("Стандартные опции:\n");
    printf("  -P dir         Каталог с плагинами\n");
    printf("  -A             Объединение условий по 'И' (по умолчанию)\n");
    printf("  -O             Объединение условий по 'ИЛИ'\n");
    printf("  -N             Инвертировать условие поиска\n");
    printf("  -v, --version  Вывести версию\n");
    printf("  -h, --help     Вывести справку\n\n");
    
    printf("Опции загруженных плагинов:\n");
    for (size_t i = 0; i < plugins_count; i++) {
        printf("--- Плагин %s (%s, %s) ---\n", plugins[i].name, 
               plugins[i].info.plugin_purpose, plugins[i].info.plugin_author);
        for (size_t j = 0; j < plugins[i].info.sup_opts_len; j++) {
            printf("  --%s\t%s\n", plugins[i].info.sup_opts[j].opt.name, 
                   plugins[i].info.sup_opts[j].opt_descr);
        }
    }
}

void load_plugins(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) {
        if (debug_mode) fprintf(stderr, "[MAIN] Не удалось открыть папку плагинов: %s\n", dir_path);
        return;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        size_t len = strlen(dir->d_name);
        if (len > 3 && strcmp(dir->d_name + len - 3, ".so") == 0) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dir_path, dir->d_name);
            
            void *handle = dlopen(path, RTLD_LAZY);
            if (!handle) {
                if (debug_mode) fprintf(stderr, "[MAIN] Ошибка загрузки %s: %s\n", path, dlerror());
                continue;
            }

            Plugin p = {0};
            p.handle = handle;
            p.name = strdup(dir->d_name);
            p.get_info = dlsym(handle, "plugin_get_info");
            p.process_file = dlsym(handle, "plugin_process_file");

            if (!p.get_info || !p.process_file || p.get_info(&p.info) < 0) {
                if (debug_mode) fprintf(stderr, "[MAIN] Неверный API в плагине %s\n", path);
                dlclose(handle);
                free(p.name);
                continue;
            }

            p.active_opts_cap = 4;
            p.active_opts = malloc(p.active_opts_cap * sizeof(struct option));
            p.active_opts_len = 0;
            
            plugins = realloc(plugins, (plugins_count + 1) * sizeof(Plugin));
            plugins[plugins_count++] = p;
            if (debug_mode) fprintf(stderr, "[MAIN] Успешно загружен плагин: %s\n", p.name);
        }
    }
    closedir(d);
}

int ftw_cb(const char *fpath, const struct stat *sb, int typeflag) {
    (void)sb; 
    
    if (typeflag == FTW_DNR) {
        if (debug_mode) fprintf(stderr, "[MAIN] Ошибка доступа к каталогу: %s\n", fpath);
        return 0;
    }
    
    if (typeflag != FTW_F) return 0; 

    bool final_res = (global_op == OP_AND) ? true : false;
    bool plugins_ran = false;
    bool plugin_error = false;

    for (size_t i = 0; i < plugins_count; i++) {
        if (plugins[i].active_opts_len > 0) {
            plugins_ran = true;
            int res = plugins[i].process_file(fpath, plugins[i].active_opts, plugins[i].active_opts_len);
            
            if (res < 0) {
                plugin_error = true;
                break;
            }

            bool match = (res == 0); 

            if (global_op == OP_AND) final_res = final_res && match;
            else final_res = final_res || match;
        }
    }

    if (!plugins_ran || plugin_error) return 0; 

    if (invert_result) final_res = !final_res; 

    if (final_res) {
        printf("%s\n", fpath);
    }
    return 0;
}

char* get_exe_dir() {
    static char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        char *last_slash = strrchr(buf, '/');
        if (last_slash) *last_slash = '\0';
        return buf;
    }
    return ".";
}

int main(int argc, char *argv[]) {
    debug_mode = (getenv("LAB2DEBUG") != NULL);
    char *plugin_dir = get_exe_dir();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            plugin_dir = argv[i + 1];
            break;
        } else if (strncmp(argv[i], "-P", 2) == 0 && strlen(argv[i]) > 2) {
            plugin_dir = argv[i] + 2; 
            break;
        }
    }

    // Загружаем плагины 
    load_plugins(plugin_dir);

    // Считаем общее количество опций
    size_t total_long_opts = 2; 
    for (size_t i = 0; i < plugins_count; i++) {
        total_long_opts += plugins[i].info.sup_opts_len;
    }

    struct option *long_opts = calloc(total_long_opts + 1, sizeof(struct option));
    opt_to_plugin_map = calloc(total_long_opts + 1000, sizeof(int)); 

    long_opts[0] = (struct option){"help", no_argument, 0, 'h'};
    long_opts[1] = (struct option){"version", no_argument, 0, 'v'};

    size_t opt_idx = 2;
    int current_val = 1000; 

    // Заполняем массив long_opts
    for (size_t i = 0; i < plugins_count; i++) {
        for (size_t j = 0; j < plugins[i].info.sup_opts_len; j++) {
            struct option o = plugins[i].info.sup_opts[j].opt;
            o.val = current_val; 
            long_opts[opt_idx++] = o;
            opt_to_plugin_map[current_val - 1000] = i; 
            current_val++;
        }
    }

    int opt;
    optind = 1;
    opterr = 1;
    
    while ((opt = getopt_long(argc, argv, "P:AONvh", long_opts, NULL)) != -1) {
        if (opt == 'h') {
            print_help(argv[0]); 
            return 0;
        } else if (opt == 'v') {
            printf("Версия: 2.0\nСвиржевский А.Д., N3252\n");
            return 0;
        } else if (opt == 'A') {
            global_op = OP_AND;
        } else if (opt == 'O') {
            global_op = OP_OR;
        } else if (opt == 'N') {
            invert_result = true;
        } else if (opt == 'P') {
            // Уже обработано в ручном цикле выше
        } else if (opt >= 1000) {
            int p_idx = opt_to_plugin_map[opt - 1000];
            Plugin *p = &plugins[p_idx];
            
            if (p->active_opts_len == p->active_opts_cap) {
                p->active_opts_cap *= 2;
                p->active_opts = realloc(p->active_opts, p->active_opts_cap * sizeof(struct option));
            }
            
            struct option active = {0};
            int lo_idx = opt - 1000 + 2; 
            active.name = long_opts[lo_idx].name; 
            active.has_arg = long_opts[lo_idx].has_arg; 
            active.flag = (int *)optarg; 
            
            p->active_opts[p->active_opts_len++] = active;
        } else {
            return 1; 
        }
    }

    // Определение каталога для поиска
    const char *target_dir = ".";
    if (optind < argc) {
        target_dir = argv[optind];
    } else {
        print_help(argv[0]); 
        return 0; 
    }

    // Обход файлов
    if (ftw(target_dir, ftw_cb, 20) == -1) {
        perror("ftw");
    }

    // Очистка памяти
    for (size_t i = 0; i < plugins_count; i++) {
        free(plugins[i].name);
        free(plugins[i].active_opts);
        dlclose(plugins[i].handle);
    }
    free(plugins);
    free(long_opts);
    free(opt_to_plugin_map);

    return 0;
}
