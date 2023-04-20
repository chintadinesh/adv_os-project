#ifndef __PARSE_OPTS__

#include <unistd.h>
struct opts {
    int opt_random;
    int opt_synchronous;
    int opt_direct;
    int opt_latency;
    int opt_poll;
};

void parse_opts(int argc, char *argv[]);
void print_opts(void);
#endif
