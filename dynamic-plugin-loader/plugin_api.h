#ifndef _PLUGIN_API_H
#define _PLUGIN_API_H

#include <getopt.h>
#include <stddef.h>

#define PLUGIN_API_VERSION  1

struct plugin_option {
    struct option opt;
    const char *opt_descr;
};

struct plugin_info {
    const char *plugin_purpose;
    const char *plugin_author;
    size_t sup_opts_len;
    struct plugin_option *sup_opts;
};

int plugin_get_info(struct plugin_info* ppi);
int plugin_process_file(const char *fname, struct option in_opts[], size_t in_opts_len);

#endif
