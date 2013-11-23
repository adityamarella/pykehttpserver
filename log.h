#ifndef __DLOG_H__
#define __DLOG_H__
#include <stdio.h>
#include "pyke_time.h"

extern FILE *__log_fp;
char __log_buf__[64];

#ifdef __DEBUG
#define LOGI(fmt, args...) do {\
    if(__log_fp!=NULL) {\
        fprintf(__log_fp, "%s:INFO:%s:%d: " fmt, mytime_get_formatted_time_str(__log_buf__, sizeof(__log_buf__)), \
                __FILE__, __LINE__ , ##args);\
        fflush(__log_fp);\
    }\
    }while(0)


#define LOGE(fmt, args...) do {\
    if(__log_fp!=NULL) {\
        fprintf(__log_fp, "%s:ERROR:%s:%d: " fmt, mytime_get_formatted_time_str(__log_buf__, sizeof(__log_buf__)), \
                __FILE__, __LINE__ , ##args);\
        fflush(__log_fp);\
    }\
    }while(0)

#else
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

int init_log(char *filename);
int deinit_log();

#endif /*__DLOG_H__*/
