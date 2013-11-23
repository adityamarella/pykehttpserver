#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <stdio.h>
#include <arpa/inet.h>
#include "pyke_ssl.h"

#define MAX_REQUEST_SIZE 8192

typedef struct Connection {

    int fd;
    int size;
    int sent;
    int replayfd;//for printing out the data

    /*Start: SSL variables*/
    int is_ssl;
    PykeSSLClient ctxt;  
    /*End: SSL variables*/

    /*Start CGI vars*/
    int is_cgi_request;
    int pipe_fd;
    /*End CGI vars*/

    char client_ip[INET6_ADDRSTRLEN];
    char buffer[MAX_REQUEST_SIZE];

    struct Connection *next, *prev;

} Connection;


Connection* add_connection(Connection *c, PykeSSLClient **ctxt, int fd, char *client_ip, int is_ssl);
int del_connection(Connection *c, int fd);
int del_connection_object(Connection *c); 
int delall_connection(Connection *c);

#endif
