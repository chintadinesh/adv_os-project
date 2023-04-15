#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/times.h>

#include "debug.h"
#include "time_lib.h"

#define BUF_SIZE 128 * 1024

int copy_file(FILE *src, FILE *dst)
{
    char buffer[BUF_SIZE];
    size_t bytesRead, bytesWritten;
    DEBUG("args: src dst\n");

    while ((bytesRead = fread(buffer, 1, BUF_SIZE, src)) > 0) {
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

    start_timer();

    src = fopen(argv[1], "rb");  // Open source file for reading in binary mode
    if (src == NULL) {
        perror("Failed to open source file");
        return 1;
    }

    dst = fopen(argv[2], "wb");  // Open destination file for writing in binary mode
    if (dst == NULL) {
        perror("Failed to open destination file");
        fclose(src);
        return 1;
    }

    for(int i = 0; i < 1; i++){
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

    return 0;
}
