
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "log.h"
#include "pyke_cgi_handler.h"
#include "pyke_socket.h"
#include "pyke_socket_connection.h"

#define CGI_IDENTIFIER "/cgi/"

int pyke_cgi_handler_is_cgi_request(const char *request_uri)
{
    return NULL != strstr(request_uri, CGI_IDENTIFIER);
}

static int pyke_cgi_handler_parse_request_uri(char *uri, int uri_len, char **query_string)
{
    int i;
    uri[uri_len]='\0';

    for(i=0; i<uri_len; i++) {
        if(uri[i] == '?') {
            if(i!=uri_len-1)
                *query_string = uri + i + 1;
            uri[i] = '\0';
        }
    }
    return 0;
}

static int pyke_cgi_handler_get_env(PykeRequestHeaders *headers, char *method, int is_ssl, 
        char *query_string, char *request_uri, char* port, 
        char *client_ip, char ***env)
{
    char **tmp_env;
    char buf[1024];
    char https[2]={'0','\0'};

    if(is_ssl)
        https[0] = '1';

    char *keys[] = {
        "CONTENT_LENGTH",
        "CONTENT_TYPE",
        "GATEWAY_INTERFACE", 
        "PATH_INFO",
        "QUERY_STRING",
        "REMOTE_ADDR",
        "REQUEST_METHOD",
        "REQUEST_URI",
        "SCRIPT_NAME",
        "SERVER_PORT",
        "SERVER_PROTOCOL",
        "SERVER_SOFTWARE",
        "HTTP_ACCEPT",
        "HTTP_REFERER",
        "HTTP_ACCEPT_ENCODING",
        "HTTP_ACCEPT_LANGUAGE",
        "HTTP_ACCEPT_CHARSET",
        "HTTP_HOST",
        "HTTP_COOKIE",
        "HTTP_USER_AGENT",
        "HTTP_CONNECTION",
        "HTTPS"
    };

    char *values[] = {
        headers->HDR_CONTENT_LENGTH,
        headers->HDR_CONTENT_TYPE,
        "CGI/1.1",
        request_uri+strlen(CGI_IDENTIFIER),
        query_string,
        client_ip,
        method,
        request_uri,
        "/cgi",
        port,
        HTTP_VERSION,
        SERVER_NAME,
        headers->HDR_ACCEPT,
        headers->HDR_REFERER,
        headers->HDR_ACCEPT_ENCODING,
        headers->HDR_ACCEPT_LANGUAGE,
        headers->HDR_ACCEPT_CHARSET,
        headers->HDR_HOST,
        headers->HDR_COOKIE,
        headers->HDR_USER_AGENT,
        headers->HDR_CONNECTION,
        https,  
    };

    int length = sizeof(values)/sizeof(char*); 
    int i = 0;


    tmp_env = (char**)calloc(1+length, sizeof(char*));

    for(i=0; i<length; i++) {
        int n; 
        n = snprintf(buf, sizeof(buf), "%s=%s", keys[i], values[i]==NULL?"":values[i]);

        if(n==sizeof(buf)) {
            buf[n-1] = '\0';
            n--;
        }
        
        char *tmp = (char*)calloc(n+1, sizeof(char));
        if(tmp == NULL) {
            fprintf(stderr, "Calloc error!!\n");
            break;
        }
        memcpy(tmp, buf, n+1);

        tmp_env[i] = tmp;
    }
    tmp_env[i] = NULL;
    *env = tmp_env;
    return 0;
}

int pyke_cgi_handler_spawn(PykeHttp *http, Connection *c, PykeRequestHeaders *h, char *method, char *request_uri, char* post_data)
{
    /* imagine a channel with pyke process on the left and cgi process on right; 
     * forwardpipe pyke -> cgi (arrow represents the direction of writes)
     * backwardpipe pyke <- cgi
     * */
    int forwardpipe[2]; 
    int backwardpipe[2];
    int pid;
    int error;

    char buffer[1024];
    char *cgi_script_path;
    char *query_string;

    int request_uri_length;

    request_uri_length = strlen(request_uri);
    snprintf(buffer,sizeof(buffer), "%s", request_uri);

    cgi_script_path = http->cgi_script_path;
    pyke_cgi_handler_parse_request_uri(buffer, request_uri_length, &query_string);

    error = pipe(forwardpipe);
    if(error<0) {
        LOGE("Error creating forward pipe %d\n", errno);
        return -1;
    }
    error = pipe(backwardpipe);
    if(error<0) {
        LOGE("Error creating backward pipe %d\n", errno);
        return -1;
    }

    LOGI("Forking CGI process :%s for client: %s\n", cgi_script_path, c->client_ip);

    pid = fork(); 
    if(pid == 0) {
        /*child process*/
        char *newargv[2] = {cgi_script_path, NULL};
        char **env = NULL;

        char port_buf[16];

        if(c->is_ssl)
            snprintf(port_buf, sizeof(port_buf), "%d", http->https_port);
        else
            snprintf(port_buf, sizeof(port_buf), "%d", http->port);

        pyke_cgi_handler_get_env(h, method, c->is_ssl, query_string, request_uri, port_buf, c->client_ip, &env);

        close(forwardpipe[1]);
        close(backwardpipe[0]);

        dup2(forwardpipe[0], STDIN_FILENO);
        dup2(backwardpipe[1], STDOUT_FILENO);

        error = execve(cgi_script_path, newargv, env);
        if(error < 0) {
            char buf[256];
            int ret = 0;
            ret = snprintf(buf, sizeof(buf), "%s\nexecve(%s, newargv, env) failed with errno: %d", HTTP_500, cgi_script_path, errno);
            fprintf(stdout, "HTTP/1.1 %s\r\nContent-Length: %d\r\n\r\n%s", HTTP_500, ret, buf);
            exit(EXIT_FAILURE);
        }
    
    } else {
        /*parent process*/
        close(forwardpipe[0]);
        close(backwardpipe[1]);

        /* write request 'data' to child
         * add write fd to select and write when next time select comes out?
         */
        write(forwardpipe[1], post_data, strlen(post_data));
        close(forwardpipe[1]);

        /*add the read end of the pipe to select*/
        c->is_cgi_request = 1;
        c->pipe_fd = backwardpipe[0];
        pyke_socket_add_read_fd(&http->s, c->pipe_fd);
    }
    return 0;
}
