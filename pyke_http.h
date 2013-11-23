#ifndef __PYKE_HTTP_H__
#define __PYKE_HTTP_H__

#include "pyke_socket.h"

typedef struct {
    int port;
    int https_port;
    char wwwfolder[256];
    char cgi_script_path[256];
    PykeSocket s;
} PykeHttp;

typedef struct {
    char * HDR_CONTENT_LENGTH;
    char * HDR_CONTENT_TYPE;
    char * HDR_ACCEPT;
    char * HDR_REFERER;
    char * HDR_ACCEPT_ENCODING;
    char * HDR_ACCEPT_LANGUAGE;
    char * HDR_ACCEPT_CHARSET;
    char * HDR_HOST;
    char * HDR_COOKIE;
    char * HDR_USER_AGENT;
    char * HDR_CONNECTION;
} PykeRequestHeaders;

#define DEFAULT_MIME "text/html"

#define HTTP_200 "200 OK"
#define HTTP_400 "400 Bad Request"
#define HTTP_404 "404 Not Found"
#define HTTP_403 "403 Forbidden"
#define HTTP_411 "411 Length Required"
#define HTTP_500 "500 Internal Server Error"
#define HTTP_501 "501 Not Implemented"
#define HTTP_503 "503 Service Unavailable"
#define HTTP_505 "505 HTTP Version Not Supported"

#define HTTP_VERSION "HTTP/1.1"
#define HTTP_LINE_DELIM "\r\n"
#define HTTP_DELIM ": "
#define HTTP_FIRST_LINE_DELIM " "
#define GET "GET"
#define POST "POST"
#define HEAD "HEAD"

#define SERVER_NAME "Pyke/1.0"
#define HTTP_HEADER_CONNECTION "Connection"
#define HTTP_HEADER_DATE "Date"
#define HTTP_HEADER_MODIFIED_DATE "Last-Modified"
#define HTTP_HEADER_SERVER "Server"
#define HTTP_HEADER_CONTENT_TYPE "Content-Type"
#define HTTP_HEADER_CONTENT_LENGTH "Content-Length"
#define HTTP_HEADER_ACCEPT "Accept"
#define HTTP_HEADER_REFERRER "Referrer"
#define HTTP_HEADER_ACCEPT_ENCODING "Accept-Encoding"
#define HTTP_HEADER_ACCEPT_LANGUAGE "Accept-Language"
#define HTTP_HEADER_ACCEPT_CHARSET "Accept-Charset"
#define HTTP_HEADER_HOST "Host"
#define HTTP_HEADER_COOKIE "Cookie"
#define HTTP_HEADER_USER_AGENT "User-Agent"


int pyke_http_init(PykeHttp *h, int http_port, int https_port, char *wwwfolder, 
        char *cgi_script_path, char *private_key_file, char *public_key_file); 

int pyke_http_run_server(PykeHttp *h);

int pyke_http_close(PykeHttp *h);

#endif
