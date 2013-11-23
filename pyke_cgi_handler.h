#ifndef __PYKE_CGI_HANDLER_H__
#define __PYKE_CGI_HANDLER_H__

#include "pyke_http.h"

typedef struct {


} PykeCGIHandler;

#define CONTENT_LENGTH "CONTENT_LENGTH"
#define CONTENT_TYPE "CONTENT_TYPE"
#define GATEWAY_INTERFACE "GATEWAY_INTERFACE"
#define PATH_INFO "PATH_INFO"
#define QUERY_STRING "QUERY_STRING"
#define REMOTE_ADDR "REMOTE_ADDR"
#define REQUEST_METHOD "REQUEST_METHOD"
#define REQUEST_URI "REQUEST_URI"
#define SCRIPT_NAME "SCRIPT_NAME"
#define SERVER_PORT "SERVER_PORT"
#define SERVER_PROTOCOL "SERVER_PROTOCOL"
#define SERVER_SOFTWARE "SERVER_SOFTWARE"

int pyke_cgi_handler_is_cgi_request(const char *request_uri);
int pyke_cgi_handler_spawn(PykeHttp *http, Connection *c, PykeRequestHeaders *h, char *method, char *request_uri, char *post_data);

#endif
