
#include <stdio.h>
#include <liburing.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>


#include "iolib.h"
#include "debug.h"

int setup_context(unsigned entries, struct io_uring *ring) 
{
    int ret;
    DEBUG("Setting up context with entries = %d, ring ptr = %p\n", entries, (void *)ring);

    ret = io_uring_queue_init(entries, ring, 0);
    if( ret < 0) {
        fprintf(stderr, "queue_init: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

int get_file_size(int fd, off_t *size) 
{
    struct stat st;

    if (fstat(fd, &st) < 0 )
        return -1;
    if(S_ISREG(st.st_mode)) {
        *size = st.st_size;
        return 0;
    } else if (S_ISBLK(st.st_mode)) {
        unsigned long long bytes;

        if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
            return -1;

        *size = bytes;
        return 0;
    }
    return -1;
}

void queue_prepped(struct io_uring *ring, struct io_data *data) 
{
    struct io_uring_sqe *sqe;

    DEBUG("args: ring = %p, data = %p, with %s\n", 
                ring, data, (data->read)? "read": "write");
    sqe = io_uring_get_sqe(ring); // get a new entry
    assert(sqe);

    // interesting!
    // TODO: There is only one queue for both reads and writes?
    // can we have more than one queue?
    if (data->read) // we change the file descriptors here
        io_uring_prep_readv(sqe, 
                            data->fop->infd, 
                            &data->iov, // TODO: Again, what is the role of iov 
                            1,  //nr_vecs // TODO: What is this for
                            data->offset);
    else
        io_uring_prep_writev(sqe, 
                            data->fop->outfd, 
                            &data->iov, 
                            1, 
                            data->offset);

    io_uring_sqe_set_data(sqe, data); // set the data field in the sque
}

int queue_read(struct io_uring *ring, 
                off_t size, 
                off_t offset, 
                struct io_fop *fop) 
{
    struct io_uring_sqe *sqe;
    struct io_data *data;

    DEBUG("args: ring = %p, size = %ld, offset = %ld\n", ring, size, offset);

    data = malloc(size + sizeof(*data));
    if (!data)
        return 1;

    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        free(data);
        return 1;
    }

    data->read = 1;
    data->offset = data->first_offset = offset;

    data->iov.iov_base = data + 1;
    data->iov.iov_len = size;
    data->first_len = size;
    data->fop = fop;

    io_uring_prep_readv(sqe, 
                        data->fop->infd, 
                        &data->iov, 
                        1, 
                        offset);

    io_uring_sqe_set_data(sqe, 
                            data);

    return 0;
}

void queue_write(struct io_uring *ring, struct io_data *data) 
{

    DEBUG("args: ring = %p, data = %p\n", (void *)ring, (void *)data);
    data->read = 0;
    data->offset = data->first_offset;

    data->iov.iov_base = data + 1; // the data starts at the end of data
    data->iov.iov_len = data->first_len;

    queue_prepped(ring, data);
    io_uring_submit(ring);
}
