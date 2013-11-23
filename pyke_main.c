
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "log.h"
#include "pyke_http.h"

#define BACKLOGS 20

typedef struct {

    int http_port;
    int https_port;
    char *lock_file;
    char *log_file;
    char *wwwfolder;
    char *cgi_script_path;
    char *private_key_file;
    char *public_key_file;

} InputParams;

PykeHttp gHttp;
int flag = 1;

/*
 * SIGINT signal handler
 *
 * @signo tells you signal useful when this function 
 * is used as signal handler for multiple signals
 * */
void pyke_shutdown(int signo)
{
    if(flag) {
        LOGI("Closing server socket. Exiting cleanly. Signo %d\n\n\n", signo);
        pyke_http_close(&gHttp);
        deinit_log();
        flag = 0;
    }
}

int pyke_init(InputParams *params)
{
    //initing server
    init_log(params->log_file);
    
    LOGI("Successfully daemonized pyked process, pid %d \n", getpid() );

    pyke_http_init(&gHttp, params->http_port, params->https_port, params->wwwfolder, 
            params->cgi_script_path, params->private_key_file, params->public_key_file);

    //start server
    pyke_http_run_server(&gHttp);

    pyke_http_close(&gHttp);
    return 0;
}

/**
 * internal signal handler
 */
void signal_handler(int sig)
{
        switch(sig)
        {
                case SIGHUP:
                        /* rehash the server */
                        break;          
                case SIGTERM:
                        /* finalize and shutdown the server */
                        // TODO: pyke_shutdown(NULL, EXIT_SUCCESS);
                        pyke_shutdown(sig);
                        break;    
                default:
                        break;
                        /* unhandled signal */      
        }       
}

/** 
 * internal function daemonizing the process
 */
int daemonize(InputParams *params)
{
    /* drop to having init() as parent */
    int i, lfp, pid = fork();
    char str[256] = {0};
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    setsid();

    for (i = getdtablesize(); i>=0; i--)
            close(i);

    i = open("/dev/null", O_RDWR);
    dup(i); /* stdout */
    dup(i); /* stderr */
    umask(027);

    lfp = open(params->lock_file, O_RDWR|O_CREAT|O_EXCL, 0640);
    
    if (lfp < 0)
        exit(EXIT_FAILURE); /* can not open */

    if (lockf(lfp, F_TLOCK, 0) < 0)
        exit(EXIT_SUCCESS); /* can not lock */
    
    /* only first instance continues */
    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); /* record pid to lock_file */

    signal(SIGCHLD, SIG_IGN); /* child terminate signal */
    signal(SIGPIPE, SIG_IGN); /* currently SSL_shutdown is attempting to write to a broken pipe causing SIGPIPE*/

    signal(SIGHUP, signal_handler); /* hangup signal */
    signal(SIGTERM, signal_handler); /* software termination signal from kill */

    pyke_init(params);

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    char *endptr;
    int http_port, https_port = 0;
    InputParams params;

    if(argc < 9) {
        //only <HTTP port> and <log file> are being used for Checkpoint-1
        fprintf(stderr, "Usage: ./pyked <HTTP port> <HTTPS port> <log ﬁle> <lock_file> <wwwfolder> <CGI script path> <private key file> <certificate file>\n");
        return 1;
    }

    memset(&params, 0, sizeof(InputParams));

    //Input validation follows
    //Port number should an integer between 0-65536
    http_port = strtol(argv[1], &endptr, 10);
    if( !(argv[1]!='\0' && *endptr=='\0') || 
        http_port<0 || http_port >= (1<<16 )
        ) {
        fprintf(stderr, "%s is an invalid port number. Port number should be an integer between 0-%d\n", 
                argv[1], (1<<16));
        return 0;
    }

    https_port = strtol(argv[2], &endptr, 10);
    if( !(argv[2]!='\0' && *endptr=='\0') || 
        https_port<0 || https_port >= (1<<16 )
        ) {
        fprintf(stderr, "%s is an invalid port number. Port number should be an integer between 0-%d\n", 
                argv[2], (1<<16));
        return 0;
    }

    params.http_port = http_port;
    params.https_port = https_port;
    params.lock_file = argv[4];
    params.log_file = argv[3];
    params.wwwfolder = argv[5];
    params.cgi_script_path = argv[6];
    params.private_key_file = argv[7];
    params.public_key_file = argv[8];

    signal(SIGPIPE, SIG_IGN); /* currently SSL_shutdown is attempting to write to a broken pipe causing SIGPIPE*/
    //daemonize(&params);
    pyke_init(&params);
    return 0;
}
