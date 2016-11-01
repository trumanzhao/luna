/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#include <stdarg.h>
#include <assert.h>
#include <string.h>
#if defined(__linux) || defined(__APPLE__)
#include <unistd.h>
#endif
#include <mutex>
#include "tools.h"
#include "log.h"

#define LOG_DIR         "logs"
#define LOG_LINE_MAX    (1024 * 2)
#define LOG_MAX_PATH	1024

struct LogParam
{
    FILE* pFile;
    int nLineCount;
    int nMaxFileLine;
    void* pvUsrData;
    XLogCallback* pCallback;
	std::mutex mutex;
	char szDir[LOG_MAX_PATH];
};

static LogParam*    g_pLog       = NULL;
static const char*  g_pszLevel[] =
{
    "Err",
    "War",
    "Inf",
    "Dbg"
};

bool LogOpen(const char cszName[], int nMaxLine)
{
    bool    bResult     = false;
    int     nRetCode    = false;
    char*   pszRet      = NULL;
    char    szCWD[LOG_MAX_PATH];

    assert(g_pLog == NULL);

    pszRet = getcwd(szCWD, sizeof(szCWD));
    if (pszRet == nullptr)
		return false;

    nRetCode = snprintf(g_pLog->szDir, sizeof(g_pLog->szDir), "%s/%s/%s", szCWD, LOG_DIR, cszName);
    if (nRetCode <= 0 || nRetCode >= (int)sizeof(g_pLog->szDir))
		return false;

    g_pLog = new LogParam;
    g_pLog->pFile           = NULL;
    g_pLog->nLineCount      = 0;
    g_pLog->nMaxFileLine    = nMaxLine;
    g_pLog->pCallback       = NULL;
    g_pLog->pvUsrData       = NULL;

    return true;
}

void LogClose()
{
    if (g_pLog == NULL)
        return;

    if (g_pLog->pFile)
    {
        fclose(g_pLog->pFile);
        g_pLog->pFile = NULL;
    }

    delete g_pLog;
	g_pLog = nullptr;
}

static FILE* CreateLogFile(const char cszPath[])
{
    int     nRetCode    = 0;
    char*   pszBuffer   = strdup(cszPath);
    char*   pszPos      = pszBuffer;
    int     nPreChar    = 0;

    while (*pszPos)
    {
        if (*pszPos == '/' || *pszPos == '\\')
        {
            *pszPos = '\0';

            if (nPreChar != 0 && nPreChar != ':')
            {
                nRetCode = make_dir(pszBuffer);
                if (nRetCode != 0 && errno != EEXIST)
					return nullptr;
            }

            *pszPos = '/';
        }

        nPreChar = *pszPos++;
    }

    FILE* pFile = fopen(pszBuffer, "w");
    free(pszBuffer);
    return pFile;
}


static bool ResetLogFile()
{
    bool    bResult     = false;
    int     nRetCode    = false;
    time_t  uTimeNow    = time(nullptr);
    tm timeNow;
    FILE*   pFile       = NULL;
    char    szPath[LOG_MAX_PATH];

    assert(g_pLog);

    localtime_r(&uTimeNow, &timeNow);

    nRetCode = snprintf(
        szPath, sizeof(szPath),
        "%s/%2.2d_%2.2d_%2.2d_%2.2d_%2.2d.log",
        g_pLog->szDir,
        timeNow.tm_mon + 1,
        timeNow.tm_mday,
        timeNow.tm_hour,
        timeNow.tm_min,
        timeNow.tm_sec
    );
    if (nRetCode <= 0 || nRetCode >= (int)sizeof(szPath))
		return false;

    pFile = CreateLogFile(szPath);
    if (pFile == nullptr)
		return false;

    if (g_pLog->pFile)
    {
        fclose(g_pLog->pFile);
        g_pLog->pFile = NULL;
    }

    g_pLog->pFile = pFile;
    g_pLog->nLineCount = 0;

    return true;
}

void Log(LogType eType, const char cszFormat[], ...)
{
    int nRetCode = 0;
    time_t uTimeNow = time(nullptr);
    tm timeNow;
    uint64_t thread_id = get_thread_id();
    int             nLogLen     = 0;
    XLogCallback*   pCallback   = NULL;
    va_list         marker;
    static char     s_szLog[LOG_LINE_MAX];

    if (g_pLog == nullptr)
        return;

    assert(eType >= eLogError);
    assert(eType <= eLogDebug);

    localtime_r(&uTimeNow, &timeNow);

    g_pLog->mutex.lock();

    nLogLen = snprintf(
        s_szLog, sizeof(s_szLog),
        "%2.2d-%2.2d,%2.2d:%2.2d:%2.2d<%s,%llu>: ",
        timeNow.tm_mon + 1,
        timeNow.tm_mday,
        timeNow.tm_hour,
        timeNow.tm_min,
        timeNow.tm_sec,
        g_pszLevel[eType],
        thread_id
    );

    assert(nLogLen > 0);
    assert(nLogLen < sizeof(s_szLog));

    va_start(marker, cszFormat);
    nRetCode = vsnprintf(s_szLog + nLogLen, sizeof(s_szLog) - nLogLen, cszFormat, marker);
    va_end(marker);

    if (nRetCode > 0 && nRetCode < (int)sizeof(s_szLog) - nLogLen)
    {
        nLogLen += nRetCode;
    }

    if (nLogLen + 1 < sizeof(s_szLog) && s_szLog[nLogLen - 1] != '\n')
    {
        s_szLog[nLogLen++] = '\n';
        s_szLog[nLogLen] = '\0';
    }

    pCallback = g_pLog->pCallback;
    if (pCallback != NULL)
    {
        nRetCode = (*pCallback)(g_pLog->pvUsrData, eType, s_szLog);
        if (!nRetCode)
        {
            g_pLog->mutex.unlock();
            return;
        }
    }
    else
    {
        fwrite(s_szLog, nLogLen, 1, stdout);
        fflush(stdout);
    }

    if (g_pLog->pFile == NULL || g_pLog->nLineCount >= g_pLog->nMaxFileLine)
    {
        ResetLogFile();
    }

    if (g_pLog->pFile)
    {
        fwrite(s_szLog, nLogLen, 1, g_pLog->pFile);
        fflush(g_pLog->pFile);
        g_pLog->nLineCount++;
    }

    g_pLog->mutex.unlock();
}

void LogHook(void* pvUsrData, XLogCallback* pCallback)
{
    g_pLog->pvUsrData = pvUsrData;
    g_pLog->pCallback = pCallback;
}

