
#include <stdio.h>


FILE *__log_fp = NULL;

int init_log(char *filename)
{
    if(filename==NULL)
        return 0;
#ifdef __DEBUG
    __log_fp = fopen(filename, "a");
    if(__log_fp==NULL)
        return 1;
    else
#endif
        return 0;
}

int deinit_log() { 
#ifdef __DEBUG
    fclose(__log_fp);
#endif
    return 0;
}
