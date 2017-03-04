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
