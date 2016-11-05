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
#ifdef _MSC_VER
#include <windows.h>
#endif
#ifdef __linux
#include <unistd.h>
#include <dirent.h>
#endif
#ifdef __APPLE__
#include <unistd.h>
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#include "tools.h"

extern "C"
{
    #include "sha1/sha1.h"
}

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

#ifdef __linux
int64_t get_time_ms()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t iTime = ts.tv_sec;
    return iTime * 1000 + ts.tv_nsec / 1000 / 1000;
}
#endif


#ifdef __APPLE__
int64_t get_time_ms()
{
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    uint64_t iTime = mts.tv_sec;
    return iTime * 1000 + mts.tv_nsec / 1000 / 1000;
}
#endif

#ifdef _MSC_VER
// just for developing :)
int64_t get_time_ms() 
{
	return (int64_t)GetTickCount64(); 
}
#endif

void sleep_ms(int ms)
{
#if defined(__linux) || defined(__APPLE__)
    usleep(ms * 1000);
#endif

#ifdef _MSC_VER
    Sleep(ms);
#endif
}

char* get_error_string(char buffer[], int len, int no)
{
	buffer[0] = '\0';

#ifdef _MSC_VER
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, no, 0, buffer, len, nullptr);
#endif

#if defined(__linux) || defined(__APPLE__)
	strerror_r(no, buffer, len);
#endif

	return buffer;
}

void get_error_string(std::string& err, int no)
{
	char txt[MAX_ERROR_TXT];
	get_error_string(txt, sizeof(txt), no);
	err = txt;
}


void sha1_string(char* buffer, const void* data, size_t data_len)
{
    unsigned char hash[SHA1_STRING_SIZE / 2 + 1];

    SHA1((char*)hash, (const char*)data, (int)data_len);

    char* pos = buffer;
    for (int i = 0; i < SHA1_STRING_SIZE / 2; ++i)
    {
        pos += sprintf(pos, "%02x", hash[i]);
    }
}
