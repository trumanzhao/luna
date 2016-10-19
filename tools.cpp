/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <string>
#include <locale>
#include <cstdint>
#ifdef __linux
#include <dirent.h>
#endif
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

time_t get_file_time(const char* file_name)
{
    if (file_name == nullptr)
        return 0;

    struct stat file_info;
    int ret = stat(file_name, &file_info);
    if (ret != 0)
        return 0;

#ifdef __APPLE__
    return file_info.st_mtimespec.tv_sec;
#endif

#if defined(_MSC_VER) || defined(__linux)
    return file_info.st_mtime;
#endif
}

int64_t get_time_ms()
{
    timespec ts;

#ifdef __linux
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#endif

#ifdef __APPLE__
    // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
#endif

    uint64_t iTime = ts.tv_sec;
    return iTime * 1000 + ts.tv_nsec / 1000 / 1000;
}


