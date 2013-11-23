#ifndef __MY_TIME_H__
#define __MY_TIME_H__


int mytime_get_time_sec();

long long mytime_get_time_usec();

char *mytime_get_formatted_time_str(char *str, int len);

char *mytime_get_formatted_time_from_epoch_time(int timefromepoch, char *str, int len);

#endif
