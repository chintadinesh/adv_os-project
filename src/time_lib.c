#include <time.h>
#include <sys/times.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <liburing.h>

static struct tms tms_start, tms_end;
static clock_t start_time, end_time;
static double user_time, system_time, real_time; 

void start_timer()
{
    start_time = clock(); // Get the initial clock time
    times(&tms_start); // Get the initial process times
}

void print_timer()
{
    end_time = clock(); // Get the final clock time
    times(&tms_end); // Get the final process times

    // Calculate the time elapsed in user, system, and real time
    user_time = (double)(tms_end.tms_utime - tms_start.tms_utime) / sysconf(_SC_CLK_TCK);
    system_time = (double)(tms_end.tms_stime - tms_start.tms_stime) / sysconf(_SC_CLK_TCK);
    real_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    fprintf(stderr, "User time: %.6f seconds\n", user_time);
    fprintf(stderr, "System time: %.6f seconds\n", system_time);
    fprintf(stderr, "Real time: %.6f seconds\n", real_time);
}