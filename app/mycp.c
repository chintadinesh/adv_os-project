#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/times.h>

#include "debug.h"
#include "time_lib.h"
#include "fileaccess_lib.h"
#include "parse_opts.h"

#ifndef BS
#define BS 4 * 1024
#endif

extern struct opts opts;

char buffer[BS];
int copy_file(FILE *src, FILE *dst)
{
    size_t bytesRead, bytesWritten;
    DEBUG("args: src dst\n");

    while ((bytesRead = fread(buffer, 1, BS, src)) > 0) {
        bytesWritten = fwrite(buffer, 1, bytesRead, dst);
        if (bytesWritten != bytesRead) {
            perror("Failed to write to destination file");
            fclose(src);
            fclose(dst);
            return 1;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    FILE *src, *dst;
    int ret;


    parse_opts(argc, argv);
    print_opts();

    if(opts.opt_testing){
        WARNING("mounting the fs\n");
        if(ret = system_mount()){
            perror("mount");
            exit(1);
        }
    }

    start_timer();

    DEBUG("opening the file %s\n", opts.opt_input_path);
    src = fopen(opts.opt_input_path, "rb");  // Open source file for reading in binary mode
    if (src == NULL) {
        perror("Failed to open source file");
        return 1;
    }

    DEBUG("opening dst file %s\n", opts.opt_output_path);
    dst = fopen(opts.opt_output_path, "wb");  // Open destination file for writing in binary mode
    if (dst == NULL) {
        perror("Failed to open destination file");
        fclose(src);
        return 1;
    }

    for(int i = 0; i < 1; i++){
        DEBUG("Calling file copy\n");
        if(copy_file(src, dst))
            perror("copy file");
        else
            DEBUG("file copy successful at i = %d\n", i);
        //DEBUG("file copy successful\n");

        if(fseek(src,
            0,
            SEEK_SET) != 0) {
                perror("fseek");
                exit(EXIT_FAILURE);
            }

        if(fseek(dst,
            0,
            SEEK_SET) != 0) {
                perror("fseek");
                exit(EXIT_FAILURE);
            }
    }

    if (ferror(src)) {
        perror("Error reading from source file");
    } else if (ferror(dst)) {
        perror("Error writing to destination file");
    }

    print_timer();

    fclose(src);
    fclose(dst);

    print_timer();

    fprintf(stderr, "\n");

    if(opts.opt_testing){
        if(ret = system_umount()){
            perror("umount");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
