#ifndef __DEBUG_H__

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

// Define different debug levels as macros
#define DEBUG_LEVEL_NONE 0
#define DEBUG_LEVEL_LOW 1
#define DEBUG_LEVEL_MEDIUM 2
#define DEBUG_LEVEL_HIGH 3

#ifndef OPTDEBUG
#define DEBUG_LEVEL DEBUG_LEVEL_MEDIUM
#endif

// Helper macro to get current time as string
#define CURRENT_TIME_STRING() ({ \
    time_t t = time(NULL); \
    char *time_str = ctime(&t); \
    time_str[strlen(time_str) - 1] = '\0'; \
    time_str; \
})

#if OPTDEBUG >= DEBUG_LEVEL_HIGH
#define DEBUG(fmt, ...) do { \
    struct timeval tv; \
    gettimeofday(&tv, NULL); \
    long milliseconds = tv.tv_usec / 1000; \
    time_t current_time = time(NULL); \
    struct tm *local_time = localtime(&current_time); \
    char time_str[64]; \
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);\
    fprintf(stderr, "DEBUG: [%s.%ld ms] %s:%s:%d: " \
            fmt, time_str, milliseconds, __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
} while (0)
#else
    #define DEBUG(fmt, args...) do {} while (0)
#endif

#if OPTDEBUG >= DEBUG_LEVEL_MEDIUM
    #define LOG(fmt, args...) \
        fprintf(stderr, "LOG: " fmt, \
                ##args)
#else
    #define LOG(fmt, args...) do {} while (0)
#endif

#if OPTDEBUG >= DEBUG_LEVEL_LOW
    #define WARNING(fmt, args...) \
        fprintf(stderr, "WARNING: " fmt, \
                ##args)
#else
    #define WARNING(fmt, args...) do {} while (0)
#endif

#ifdef OPTDEBUG
    #define DEBUG_ASSERT(expr) \
        if (!(expr)) { \
            fprintf(stderr, "ASSERTION FAILED: %s:%d:%s(): %s\n", \
                    __FILE__, __LINE__, __func__, #expr); \
            exit(EXIT_FAILURE); \
        }
#else
    #define DEBUG_ASSERT(expr) do {} while (0)
#endif

#endif