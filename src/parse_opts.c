#include "parse_opts.h"
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"

struct opts opts;

void print_opts(void)
{
    WARNING("Final opts = {\n");
    WARNING("\trandom = %d \n", opts.opt_random);
    WARNING("\tsynchronous = %d\n", opts.opt_synchronous); 
    WARNING("\to_direct = %d\n", opts.opt_direct); 
    WARNING("\tlatency test = %d\n", opts.opt_latency); 
    WARNING("\tio_uring poll = %d\n", opts.opt_poll); 
    WARNING("}\n"); 
} 

void parse_opts(int argc, char *argv[]){
    int c;
    char *token;

    opts.opt_random = 0;              // default = malloc
    opts.opt_synchronous = 0;
    opts.opt_direct = 0;
    opts.opt_latency = 0;
    opts.opt_poll = 0;

    while ((c = getopt(argc, argv, "ursdlp")) != -1) {
        switch (c) {
        case 'u':
            printf("Valid options are :\n");
            printf("\t-u: usage\n");
            printf("\t-s: synchronous or io_uring, default is IO_URING\n");
            printf("\t-l: test latency instead of iops, default if iops\n");
            printf("\t-r: random access\n");
            printf("\t-d: open file in O_DIRECT to bypass page cache\n");
            printf("\t-p: enable polling for IO_URING\n");
            exit(EXIT_SUCCESS);
            break;
        case 'r':
            WARNING("Option random access enabled\n");
            opts.opt_random = 1;
            break;
        case 's':
            WARNING("Option synchronous access eanbled\n");
            opts.opt_synchronous = 1;
            break;
        case 'd':
            WARNING("Option direct mode for file eanbled\n");
            opts.opt_direct = 1;
            break;
        case 'l':
            WARNING("Option latency test enabled\n");
            opts.opt_latency = 1;
            break;
        case 'p':
            WARNING("Option io_uring poll enabled\n");
            opts.opt_poll = 1;
            break;
        default:
            break;
        }
    }

}
