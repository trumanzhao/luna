/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#pragma once

#ifdef _MSC_VER
#include <time.h>
#include <direct.h>
#include <windows.h>
using int64_t = long long;
using uint64_t = unsigned long long;
#endif

#if defined(__linux) || defined(__APPLE__)
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/stat.h>
using BYTE = unsigned char;
#endif

time_t get_file_time(const char* file_name);
int64_t get_time_ms();
void sleep_ms(int ms);

template <typename T, int N>
void safe_cpy(T (&buffer)[N], const T* str)
{
	if (str == nullptr)
	{
		buffer[0] = 0;
		return;
	}

	int n = 0;
	while (n < N && *str)
	{
		buffer[n++] = *str++;
	}

	if (n >= N)
		n--;
	buffer[n] = 0;
}

#ifdef _MSC_VER
inline struct tm* localtime_r(const time_t* timep, struct tm* result)
{
    errno_t nErr = localtime_s(result, timep);
    return (nErr == 0) ? result : nullptr;
}
#endif

#ifdef _MSC_VER
inline bool make_dir(const char cszDir[]) { return (_mkdir(cszDir) != -1); }
#endif

#if defined(__linux) || defined(__APPLE__)
inline bool make_dir(const char cszDir[]) { return (mkdir(cszDir, 0777) != -1); }
#endif


#ifdef _MSC_VER
inline uint64_t get_thread_id() { return GetCurrentThreadId(); }
#endif

#if defined(__linux) || defined(__APPLE__)
inline uint64_t get_thread_id() { return (uint64_t)pthread_self(); }
#endif


#define FAILED_JUMP(C)	\
	do	\
	{	\
		if (!(C)) goto Exit0; \
	} while (0)

#define SAFE_FREE(p)	\
    do	\
    {	\
        if (p)  \
        {   \
            free(p);    \
            (p) = nullptr;  \
        }   \
    } while (0)

#define SAFE_DELETE(p)	\
    do	\
    {	\
        if (p)  \
        {   \
            delete (p);    \
            (p) = nullptr;  \
        }   \
    } while (0)

#define SAFE_DELETE_ARRAY(p)	\
    do	\
    {	\
        if (p)  \
        {   \
            delete[] (p);    \
            (p) = nullptr;  \
        }   \
    } while (0)

#if defined(__linux) || defined(__APPLE__)
template <typename T, int N>
constexpr int _countof(T(&_array)[N]) { return N; }
#endif
