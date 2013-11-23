#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int mytime_get_time_sec()
{
    struct timeval time;
    struct timezone tzone;

    gettimeofday(&time, &tzone);
    return (time.tv_sec);
}

long long mytime_get_time_usec()
{
    struct timeval time;
    struct timezone tzone;

    gettimeofday(&time, &tzone);
    return (time.tv_sec * 1000000ULL + time.tv_usec);
}

char *mytime_get_formatted_time_str(char *str, int len)
{
    char lstr[32];
    struct tm tm;
    struct timeval tv;
    int ms;
    memset(&tm, 0, sizeof(struct tm));
    gettimeofday (&tv, NULL);
    strftime(lstr, sizeof(lstr), "%Y-%m-%d %H:%M:%S", localtime_r(&tv.tv_sec, &tm));
    ms = tv.tv_usec / 1000;
    snprintf(str,len,"%s.%d",lstr,ms);
    return (str);
}

char *mytime_get_formatted_time_from_epoch_time(int timefromepoch, char *str, int len)
{
    time_t lt = timefromepoch;
    struct tm tm;

    memset(&tm, 0, sizeof(struct tm));
    strftime(str, len, "%Y-%m-%d %H:%M:%S", localtime_r(&lt, &tm));
    return (str);
}

