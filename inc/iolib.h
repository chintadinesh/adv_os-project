#ifndef __IOLIB_H_
#define __IOLIB_H_
#include <sys/stat.h>
#include <stdlib.h>
#include <liburing.h>

struct io_fop {
    int infd;
    int outfd;
    off_t insize;
    struct io_uring *pring;
};

struct io_data {
    int read;
    struct io_fop *fop;
    off_t first_offset, offset;
    size_t first_len;
    struct iovec iov;
};


int setup_context(unsigned entries, 
                  struct io_uring *ring);

int get_file_size(int fd, 
                        off_t *size);

void queue_prepped(struct io_uring *ring, 
                  struct io_data *data);

int queue_read(struct io_uring *ring, 
                off_t size, 
                off_t offset, 
                struct io_fop *fop);

void queue_write(struct io_uring *ring, 
                  struct io_data *data);

#endif