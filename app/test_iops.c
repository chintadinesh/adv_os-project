#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <liburing.h>
#include <string.h>
#include <time.h>

#include "fileaccess_lib.h"
#include "iolib.h"
#include "debug.h"
#include "time_lib.h"
#include "parse_opts.h"

#define NUM_BLOCKS 256 *1024
#ifndef QUEUE_DEPTH
#define QUEUE_DEPTH 64
#endif

#ifdef BLOCK_SIZE
#undef BLOCK_SIZE
#define BLOCK_SIZE 4096
#endif

extern struct opts opts; 

int iops_main(int argc, char **argv) {
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    struct iovec iov;
    char *buffer;
    int fd, i, ret, opt_random, opt_synchronous;
    struct timeval start_time, submit_time, end_time;

    unsigned long long num_ios = 0;
    int to_read = NUM_BLOCKS;

    void *read_buffer = NULL;

    srand(time(NULL));

    DEBUG("IOPS main in progress: testing for latency\n");

    if(ret = system_mount()){
        perror("mount");
        exit(1);
    }

    DEBUG("successfully mounted the file system\n");

    unsigned oflags = O_RDONLY;

    oflags |= (opts.opt_direct) ? O_DIRECT : 0;

    // Open the file
    fd = open("virtualdisk/largefile", oflags);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    DEBUG("opened large file with fd = %d, with flags = %d\n", fd, oflags);

    if(opts.opt_synchronous){
        if (posix_memalign(&read_buffer, BLOCK_SIZE, BLOCK_SIZE) != 0) {
            perror("posix_memalign");
            exit(EXIT_FAILURE);
        }

        DEBUG("memory aligned at address = %p, length = 0x%x, num_blocks = %x, block_size = %x\n",
                read_buffer, 
                BLOCK_SIZE,
                1,
                BLOCK_SIZE);
    }
    else{
        unsigned long mmap_size =  NUM_BLOCKS * BLOCK_SIZE; 

        // Allocate a buffer for I/O operations
        buffer = mmap(NULL, 
                        mmap_size,
                        PROT_READ, 
                        MAP_PRIVATE | MAP_ANONYMOUS, 
                        -1, 
                        0);
        if (buffer == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        DEBUG("memory mapped at address = %p, length = 0x%x\n", buffer, NUM_BLOCKS * BLOCK_SIZE);

        // Initialize io_uring
        ret = io_uring_queue_init(QUEUE_DEPTH, &ring, IORING_SETUP_IOPOLL);
        if (ret < 0) {
            perror("io_uring_queue_init");
            return 1;
        }

        DEBUG("Initialized io_uring queue\n");
    }

    // Start the timer
    gettimeofday(&start_time, NULL);

    if(opts.opt_synchronous){ // normal copy
        for(int block_num; to_read; to_read--){
            if(opts.opt_random){
                block_num =  rand() % NUM_BLOCKS;
                DEBUG("Seeking to file offset = %x\n", block_num * BLOCK_SIZE);

                ret = lseek(fd, block_num*BLOCK_SIZE, SEEK_SET);
                if(ret < 0){
                    perror("lseek");
                    return 1;
                }
            }
            else{
                block_num = NUM_BLOCKS - to_read;
            }

            DEBUG("Reading block = %d, when to_read = %d\n", block_num, to_read);
            ret = read(fd, read_buffer, BLOCK_SIZE);
            if(ret < 0){
                perror("read");
                return 1;
            }

            num_ios++;
        }
    }
    else{ // io_uring
        for(int curr_read = 0, j = 0, block_num; 
                to_read; 
                ){
            // Submit read requests to io_uring
            for (i = 0; i < QUEUE_DEPTH; i++) {
                block_num = opts.opt_random ? rand() % NUM_BLOCKS : NUM_BLOCKS - to_read;

                DEBUG("reading block number = %d when to_read = %d\n", block_num, to_read);
                // this controls where in the file to read
                iov.iov_base = buffer + (i * BLOCK_SIZE);
                iov.iov_len = BLOCK_SIZE;

                sqe = io_uring_get_sqe(&ring);

                io_uring_prep_readv(sqe, 
                                    fd, 
                                    &iov, 
                                    1, 
                                    i * block_num * BLOCK_SIZE); 

                io_uring_sqe_set_flags(sqe, 
                                        IOSQE_FIXED_FILE);

                to_read--;
            }

            io_uring_submit(&ring); // batch submit

            // Wait for all requests to complete
            for (i = 0; i < QUEUE_DEPTH; i++) {
                ret = io_uring_wait_cqe(&ring, &cqe);
                if (ret < 0) {
                    perror("io_uring_wait_cqe");
                    return 1;
                }
                num_ios++;
                io_uring_cqe_seen(&ring, cqe);
                DEBUG("Seen number of ios = %lld\n", num_ios);
            }
        }
    }

    // End the timer
    gettimeofday(&end_time, NULL);

    double elapsed_time2 = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
    WARNING("IOPS2: (end - start) %.2f\n", num_ios / elapsed_time2);
    fprintf(stderr, " %0.2f,\n", num_ios / elapsed_time2);

    if(ret = system_umount()){
        perror("umount");
        exit(EXIT_FAILURE);
    }

    // Clean up
    if(opts.opt_synchronous){
        munmap(read_buffer, BLOCK_SIZE);
    }
    else{
        io_uring_queue_exit(&ring);
        munmap(buffer, NUM_BLOCKS * BLOCK_SIZE);
    }

    close(fd);

    return 0;
}

int main2(int argc, char *argv[]) 
{
    off_t insize;
    int ret = 0;
    struct io_uring *pring;
    struct io_fop *fop;

    start_timer();

    PRINT_ARGS();

    pring = (struct io_uring *)malloc(sizeof(struct io_uring));

    if (setup_context(QUEUE_DEPTH, pring, 0))
        return 1;

    free(pring);
    print_timer();

    DEBUG("Queue exiting\n");
    io_uring_queue_exit(pring);

    print_timer();

    return ret;
}

int latency_main(int argc, char **argv) {
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    struct iovec iov;
    char *buffer;
    int fd, i, ret, opt_random, opt_synchronous;
    struct timeval start_time, submit_time, end_time;

    void *read_buffer = NULL;

    srand(time(NULL));

    if(argc > 2){
        opt_synchronous = atoi(argv[2]);
        opt_random = atoi(argv[1]);
    }
    else if(argc > 1){
        opt_synchronous = 0;
        opt_random = atoi(argv[1]);
    }
    else{
        opt_random = 0;
        opt_synchronous = 0;
    }

    unsigned long long num_ios = 0;
    int to_read = NUM_BLOCKS;

    DEBUG("latency main in progress: testing for latency\n");

    if(ret = system_mount()){
        perror("mount");
        exit(1);
    }

    DEBUG("successfully mounted the file system\n");

    // Start the timer
    gettimeofday(&start_time, NULL);

    // Open the file
    fd = open("virtualdisk/largefile", O_RDONLY | O_DIRECT);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    DEBUG("opened large file with fd = %d\n", fd);

    int block_num = opts.opt_random ? rand() % NUM_BLOCKS : 0;


    DEBUG("reading block number = %d\n", block_num);
    if(opts.opt_synchronous){
        DEBUG("Seeking to file offset = %x\n", block_num * BLOCK_SIZE);

        ret = lseek(fd, block_num*BLOCK_SIZE, SEEK_SET);
        if(ret < 0){
            perror("lseek");
            return 1;
        }

        if (posix_memalign(&read_buffer, BLOCK_SIZE, BLOCK_SIZE) != 0) {
            perror("posix_memalign");
            exit(EXIT_FAILURE);
        }

        DEBUG("memory aligned at address = %p, length = 0x%x, num_blocks = %x, block_size = %x\n",
                read_buffer, 
                BLOCK_SIZE,
                1,
                BLOCK_SIZE);
    }
    else{

        buffer = mmap(NULL, 
                        BLOCK_SIZE, 
                        PROT_READ, 
                        MAP_PRIVATE | MAP_ANONYMOUS, 
                        -1, 
                        0);
        if (buffer == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        // Allocate a buffer for I/O operations
        DEBUG("memory mapped at address = %p, length = 0x%x, num_blocks = %x, block_size = %x\n",
                buffer, 
                BLOCK_SIZE,
                NUM_BLOCKS,
                BLOCK_SIZE);

        // Initialize io_uring
        ret = io_uring_queue_init(QUEUE_DEPTH, &ring, IORING_SETUP_IOPOLL);
        if (ret < 0) {
            perror("io_uring_queue_init");
            return 1;
        }

        DEBUG("Initialized io_uring queue\n");

        // this controls where in the file to read
        iov.iov_base = buffer; 
        iov.iov_len = BLOCK_SIZE;

        sqe = io_uring_get_sqe(&ring);

        io_uring_prep_readv(sqe, 
                            fd, 
                            &iov, 
                            1, 
                            block_num * BLOCK_SIZE); 

        io_uring_sqe_set_flags(sqe, 
                                IOSQE_FIXED_FILE);

    }
    // after submitting all the request
    gettimeofday(&submit_time, NULL);
    
    if(opts.opt_synchronous){
        DEBUG("Calling read()\n");
        int total_read = 0;
        total_read = 0;
        //for(int try = 0; try < 10 && total_read < BLOCK_SIZE; total_read += ret){
            ret = read(fd, read_buffer, BLOCK_SIZE);
            if(ret < 0){
                perror("read");
                return 1;
            }
         //   try++;
        //}

        DEBUG("total read %d bytes\n", ret);
    }
    else{
        io_uring_submit(&ring); // batch submit
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            perror("io_uring_wait_cqe");
            return 1;
        }
    }

    // End the timer
    gettimeofday(&end_time, NULL);

    if(!opts.opt_synchronous)
        io_uring_cqe_seen(&ring, cqe);

    // Print the IOPS
    double elapsed_time1 = (end_time.tv_sec - submit_time.tv_sec) + (end_time.tv_usec - submit_time.tv_usec) ;
    //double elapsed_time2 = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) ;
    WARNING("IOPS1: (end - submit) %.2fus\n", elapsed_time1);
    //WARNING(stderr, "IOPS2: (end - start) %.2fus\n", elapsed_time2);
    fprintf(stderr, " %.2f, \n", elapsed_time1);
    

    if(ret = system_umount()){
        perror("umount");
        exit(EXIT_FAILURE);
    }

    if(!opts.opt_synchronous){
        // Clean up
        io_uring_queue_exit(&ring);
        munmap(buffer, BLOCK_SIZE);
    }
    else{
        munmap(read_buffer, BLOCK_SIZE);
    }

    close(fd);

    return 0;
}

int main(int argc, char *argv[])
{
    //return main1(argc, argv);
    //return latency_main(argc, argv);
    parse_opts(argc, argv);

    PRINT_ARGS();
    print_opts();

    if(opts.opt_latency)
        return latency_main(argc, argv);
    else 
        return iops_main(argc, argv);

}

