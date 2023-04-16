#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <liburing.h>
#include <time.h>
#include <sys/times.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>

#include "debug.h"
#include "time_lib.h"

#define QD  4
#define BS (1024 * 1024)

#define BUFSIZE 4096

static int infd, outfd;
struct io_uring ring;

struct io_data {
    int read;
    off_t first_offset, offset;
    size_t first_len;
    struct iovec iov;
};

static int setup_context(unsigned entries, struct io_uring *ring) {
    int ret;
    DEBUG("Setting up context with entries = %d, ring ptr = %p\n", entries, (void *)ring);

    ret = io_uring_queue_init(entries, ring, 0);
    if( ret < 0) {
        fprintf(stderr, "queue_init: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

static int get_file_size(int fd, off_t *size) {
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

static void queue_prepped(struct io_uring *ring, struct io_data *data) {
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
                            infd, 
                            &data->iov, // TODO: Again, what is the role of iov 
                            1,  //nr_vecs // TODO: What is this for
                            data->offset);
    else
        io_uring_prep_writev(sqe, 
                            outfd, 
                            &data->iov, 
                            1, 
                            data->offset);

    io_uring_sqe_set_data(sqe, data); // set the data field in the sque
}

static int queue_read(struct io_uring *ring, off_t size, off_t offset) {
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

    io_uring_prep_readv(sqe, 
                        infd, 
                        &data->iov, 
                        1, 
                        offset);

    io_uring_sqe_set_data(sqe, 
                            data);

    return 0;
}

static void queue_write(struct io_uring *ring, struct io_data *data) {

    DEBUG("args: ring = %p, data = %p\n", (void *)ring, (void *)data);
    data->read = 0;
    data->offset = data->first_offset;

    data->iov.iov_base = data + 1;
    data->iov.iov_len = data->first_len;

    queue_prepped(ring, data);
    io_uring_submit(ring);
}

int copy_file(struct io_uring *ring, off_t insize) {
    unsigned long reads, writes; // TODO: do they track in progress reads and writes?
    struct io_uring_cqe *cqe;
    off_t write_left, offset;
    int ret;

    DEBUG("args: ring = %p, size = %ld\n", ring, insize);

    write_left = insize;
    writes = reads = offset = 0;

    // this should stuck in a while loop until all data is read
    while (insize || write_left || writes) { 
        long unsigned int had_reads, got_comp; // interesting to see variables defined in a loop

        DEBUG("insize = %ld, write_left = %ld, reads = %ld, writes = %ld\n", 
                insize, write_left, reads, writes);

        /* Queue up as many reads as we can */
        had_reads = reads;
        while (insize) { // attempt a read when data left to read
            off_t this_size = insize;

            if (reads + writes >= QD){// TODO: I don't understand this
                DEBUG("reads + writes >= QD\n");
                break;
            }

            if (this_size > BS) // read 1 block at a time or less 
                this_size = BS;
            else if (!this_size)
                break;

            if (queue_read(ring, this_size, offset))
                break; // retry in the outer loop

            // TODO: partial read detect and verify
            insize -= this_size;
            offset += this_size;
            reads++; // mark a successful read
        }

        if (had_reads != reads) { // whenever you have a new read
            // TODO: I don't understand how big the ring should be
            // TODO: what does the submit do?
            ret = io_uring_submit(ring); // submit a write of the data
            if (ret < 0) {
                fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
                break;
            }
        }

        // TODO: How do we know if the queue is full?
        /* Queue is full at this point. Let's find at least one completion */
        // TODO: Is this doing reads and writes in terms of batches?
        // What happens if we have reads and writes done by different threads?
        got_comp = 0;
        while (write_left || writes) {
            struct io_data *data;

            if (!got_comp) { // try unpacking from cqe 
                ret = io_uring_wait_cqe(ring, &cqe); // TODO: is this synchronous or async?
                got_comp = 1; // to indicate that there are completions
            } else { // once we have completions TODO: why is this important?
                ret = io_uring_peek_cqe(ring, &cqe);
                if (ret == -EAGAIN) { // Only ignore try again errors
                    DEBUG("EAGAIN peek\n");
                    cqe = NULL;
                    ret = 0;
                }
                else if (ret == -EINTR) { // Only ignore try again errors
                    DEBUG("EINTR peek\n");
                    cqe = NULL;
                    ret = 0;
                }
            }
            if (ret < 0 && ret != -EAGAIN && ret != -EINTR) { // non EAGAIN erros are bad.
                fprintf(stderr, "io_uring_peek_cqe: %s, %d\n",
                        strerror(-ret), ret);
                return 1;
            }
            if (!cqe){
                DEBUG("Give some time before retrying\n");
                break; // give some time and then try again later.
            }

            data = io_uring_cqe_get_data(cqe); //unpack the data
            if (cqe->res < 0) { // some error
                if (cqe->res == -EAGAIN) { // interesting. 
                    queue_prepped(ring, data); // TODO: Not sure what this does
                    io_uring_cqe_seen(ring, cqe); // ack the data
                    continue; // TODO: I thought this would be a break
                }
                fprintf(stderr, "cqe failed: %s\n",
                        strerror(-cqe->res));
                exit(EXIT_FAILURE); 
            } 
            else if (cqe->res != data->iov.iov_len) { // a smaller read/write
                /* short read/write; adjust and requeue */
                data->iov.iov_base += cqe->res; // TODO: What is the role of iov?
                data->iov.iov_len -= cqe->res;
                queue_prepped(ring, data); 
                io_uring_cqe_seen(ring, cqe);
                continue;
            }

            /*
             * All done. If write, nothing else to do. If read,
             * queue up corresponding write.
             * */

            if (data->read) {
                queue_write(ring, data); // TODO: writing the data. Where do we change thd fd?
                write_left -= data->first_len;
                reads--;
                writes++;
            } else {
                free(data);
                writes--;
            }
            io_uring_cqe_seen(ring, cqe);
        }
    }
    DEBUG("insize = %ld, write_left = %ld, reads = %ld, writes = %ld\n", 
            insize, write_left, reads, writes);

    return 0;
}

void copy_dir(const char* src_dir, const char* dest_dir) {
    char src_path[PATH_MAX]; 
    char dest_path[PATH_MAX];
    struct dirent *entry;
    struct stat st;

    DIR *dir;
    dir = opendir(src_dir);

    if (dir == NULL) { 
        perror("Error opening source directory"); 
        exit(EXIT_FAILURE); 
    }

    mkdir(dest_dir, 0755);

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 
            || strcmp(entry->d_name, "..") == 0) 
            continue;

        snprintf(src_path, 
                    sizeof(src_path), 
                    "%s/%s", 
                    src_dir, 
                    entry->d_name);

        snprintf(dest_path, 
                sizeof(dest_path), 
                "%s/%s", 
                dest_dir, 
                entry->d_name);

        if (entry->d_type == DT_DIR) {
            copy_dir(src_path, dest_path);
        } else if (entry->d_type == DT_REG) {

            infd = open(src_path, 
                            O_RDONLY);
                            
            if (stat(src_path, &st) == -1) { 
                perror("Error getting file status"); 
                exit(EXIT_FAILURE); 
            }

            if (infd < 0) { 
                fprintf(stderr, "Error opening source file: %s\n", strerror(errno)); 
                exit(EXIT_FAILURE); 
            }

            outfd = open(dest_path, 
                                O_WRONLY | O_CREAT | O_TRUNC, 
                                st.st_mode);

            if (outfd < 0) { 
                fprintf(stderr, "Error creating destination file: %s\n", strerror(errno)); 
                exit(EXIT_FAILURE); 
            }

            off_t insize;
            if (get_file_size(infd, &insize)){
                perror("get_file_size");
                exit(EXIT_FAILURE);
            }

            copy_file(&ring, insize);

            close(infd);
            close(outfd);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    off_t insize;
    int ret;

    start_timer();

    if (argc < 3) {
        printf("Usage: %s <infile> <outfile>\n", argv[0]);
        return 1;
    }

    DEBUG("Args: ");
#if OPTDEBUG >= DEBUG_LEVEL_HIGH
    for(int i = 0; i < argc; i++){
        if(i == argc - 1)
            fprintf(stderr, "%s \n", argv[i]);
        else
            fprintf(stderr, "%s ", argv[i]);
    }
#endif

    // Get information about the source path
    struct stat src_stat;
    if (stat(argv[1], &src_stat) == -1) {
        perror("Error getting source path information");
        exit(EXIT_FAILURE);
    }

    // Determine if the source path is a file or a directory
    if (S_ISREG(src_stat.st_mode)) { // Source path is a regular file
        infd = open(argv[1], O_RDONLY);
        if (infd < 0) {
            perror("open infile");
            exit(EXIT_FAILURE);
        }

        outfd = open(argv[2], 
                    O_WRONLY | O_CREAT | O_TRUNC, 
                    src_stat);

        if (outfd < 0) {
            perror("open outfile");
            return 1;
        }

        if (setup_context(QD, &ring))
            return 1;

        if (get_file_size(infd, &insize))
            return 1;

        DEBUG("file size = %ld\n", insize);

        for(int i = 0 ; i < 1; i++)
        {
            ret = copy_file(&ring, insize);
            DEBUG("file copy successful at i = %d\n", i);
        }

    } 
    else if (S_ISDIR(src_stat.st_mode)) {
        // Source path is a directory

        if (setup_context(QD, &ring))
            return 1;
        copy_dir(argv[1], argv[2]);
    } else {
        printf("Source path is not a regular file or directory\n");
        exit(EXIT_FAILURE);
    }

    print_timer();

    if (S_ISREG(src_stat.st_mode)) { // Source path is a regular file
        close(infd);
        DEBUG("Closed infd\n");
        close(outfd);
        DEBUG("Closed outfd\n");
    }

    DEBUG("Queue exiting\n");
    io_uring_queue_exit(&ring);

    print_timer();

    return ret;
}