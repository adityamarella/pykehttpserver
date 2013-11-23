#ifndef __PYKE_SOCKET_H__
#define __PYKE_SOCKET_H__

#include "pyke_socket_connection.h"
#include "pyke_ssl.h"

typedef struct {

    int port;
    int ssl_port;
    int sockfd;
    int ssl_sockfd;
    Connection head; /// Linked list for holding the connection state
                        /// 'head' is a dummy node, useful for holding a reference;
                        /// 'head' needs to be a global variable to free memory associated with 
                        /// each connection when SIGINT happens

    void *ctxt;         /// this is used to hold reverse reference to http object
                        /// ctxt has to be passed to the callback functions

    PykeSSL ssl;
    fd_set read_fds;
    fd_set write_fds; 
    int fdmax;

} PykeSocket;

/* initializes PykeSocket structure
 * @port is the port on which the server runs
 * */
int pyke_socket_init(PykeSocket *s, void *ctxt, int port, int ssl_port, char *private_key_file, char *public_key_file);

/* close the socket
 * */
int pyke_socket_close(PykeSocket *s);

/* send data on the socket fd
 * */
int pyke_socket_send(PykeSocket *s, Connection *c, char *data, int len);

/*
 * Run the socket server and keep handle connections until there is SIGINT
 *
 * */
int pyke_socket_run_server(PykeSocket *s);

int pyke_socket_add_read_fd(PykeSocket *s, int fd);

int pyke_socket_clr_read_fd(PykeSocket *s, int fd);

int pyke_socket_client_shutdown(PykeSocket *s, Connection *c);

#endif
