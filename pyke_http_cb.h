#ifndef __PYKE_HTTP_CB__
#define __PYKE_HTTP_CB__

#include "pyke_socket_connection.h"

/*prototypes of call back functions from socket layer go in this file*/

int http_cb_on_client_connection(void *ctxt, int fd);

int http_cb_on_data_recv(void *ctxt, Connection *c, char *data, int len);

int http_cb_on_error(void *ctxt, Connection *c, int error);

#endif
