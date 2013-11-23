
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <string.h>
#include "pyke_http.h"
#include "pyke_time.h"
#include "pyke_cgi_handler.h"
#include "log.h"

char *MIME_TABLE[5][2] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".png", "image/png"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
};

int pyke_http_init(PykeHttp *h, int http_port, int https_port, char *wwwfolder, 
        char *cgi_script_path, char *private_key_file, char *public_key_file)
{
    memset(h, 0, sizeof(PykeHttp));
    h->port = http_port;
    h->https_port = https_port;
    snprintf(h->wwwfolder, sizeof(h->wwwfolder), "%s", wwwfolder); 
    snprintf(h->cgi_script_path, sizeof(h->cgi_script_path), "%s", cgi_script_path); 
    pyke_socket_init(&h->s, h, http_port, https_port, private_key_file, public_key_file);
    return 0;
}

int pyke_http_run_server(PykeHttp *h)
{ 
    int error = 0;

    error = pyke_socket_run_server(&h->s);
    return error;
}

int pyke_http_close(PykeHttp *h)
{
    return pyke_socket_close(&h->s);
}

char *METHODS[]= {GET, POST, HEAD};

/*
 * returns -1 for version mismatch
 *          1 for GET
 *          2 for POST
 *          3 for HEAD
 * */

static char *get_file_extension (char *path)
{
    char *e = strrchr (path, '.');
    if (e == NULL)
        e = ""; // fast method, could also use &(fspec[strlen(fspec)]).
    return e;
} 

static char* get_mime_from_file_path(char *path) 
{
    int i;

    for(i=0; i<5; i++) { //hardcoding alert
        if(strcmp(get_file_extension(path), MIME_TABLE[i][0]) == 0)
            break;
    }
    if(i==5)
        return DEFAULT_MIME;
    return MIME_TABLE[i][1];
}

static int pyke_http_parse_first_line(char *line, char **request_uri)
{
    char *saveptr = {0};
    char *token;
    char *version;

    token = strtok_r(line, HTTP_FIRST_LINE_DELIM, &saveptr);
    if(token==NULL)
        return -2;
    *request_uri = strtok_r(NULL, HTTP_FIRST_LINE_DELIM, &saveptr);
    if(*request_uri==NULL)
        return -2;
    version = strtok_r(NULL, HTTP_FIRST_LINE_DELIM, &saveptr);
    if(version==NULL)
        return -2;
    if(strcmp(version, HTTP_VERSION) != 0)
        return -1;
    
    if(strcmp(token, GET) == 0) {
        return 1;
    } else if(strcmp(token, POST) == 0) {
        return 2;
    } else if(strcmp(token, HEAD) == 0) {
        return 3;
    }
    return 0;
}

typedef struct {
    
    int method;
    int content_length; //for post request
    char *request_uri;
    char *connection;

} HttpRequest;

typedef struct {

    char *buffer;
    int size;
    int offset;

} Response;

/* START UTIL
 *
 * UTIL functions used for
 * HTTP response construction
 *
 * */

static int pyke_http_add_response_firstline(Response *r, char *version, char *statusstr)
{
    int verlen = strlen(version);
    int statuslen = strlen(statusstr);

    if(r->offset + verlen + statuslen + 3 >= r->size) {
    
        int alloc_size = 4096;
        if(r->size==0) {
            r->buffer = (char*)malloc(alloc_size*sizeof(char)); 
        } else {
            r->buffer = (char*)realloc(r->buffer, r->size + alloc_size);
        }
        r->size = r->size + alloc_size;
    
    }
    snprintf(r->buffer+r->offset, r->size, "%s %s\r\n", version, statusstr);
    r->offset += verlen + statuslen + 3;
    return 0;
}

static int pyke_http_add_data(Response *r, char *data, int len)
{

    if(r->offset + len > r->size-1) {
        int alloc_size = 4096;
        if(r->size==0) {
            r->buffer = (char*)malloc(alloc_size*sizeof(char)); 
        } else {
            r->buffer = (char*)realloc(r->buffer, r->size + alloc_size);
        }
        r->size = r->size + alloc_size;
    }
    memcpy(r->buffer+r->offset, data, len);
    r->offset += len;
    return len;
}

static int pyke_http_add_response_header(Response *r, char *key, char *value)
{
    int keylen = strlen(key);
    int valuelen = strlen(value);

    if(r->offset + keylen + valuelen + 4 > r->size) {
        int alloc_size = 4096;
        if(r->size==0) {
            r->buffer = (char*)malloc(alloc_size*sizeof(char)); 
        } else {
            r->buffer = (char*)realloc(r->buffer, r->size + alloc_size);
        }
        r->size = r->size + alloc_size;
    }
    snprintf(r->buffer+r->offset, r->size, "%s: %s\r\n", key, value);
    r->offset += keylen + valuelen + 4;
    return 0;
}

static int pyke_http_add_file_data(Response *r, char *path)
{
    FILE *fp;
    char buffer[1024] = {0};
    int bytes_read = 0, bytes_added = 0;

    fp = fopen(path, "r");
    if(fp==NULL)
        return -1;
    while( (bytes_read = fread(buffer, sizeof(char), 1024, fp))>0)
        bytes_added += pyke_http_add_data(r, buffer, bytes_read);
    fclose(fp);
    return bytes_added;
}

/* b's data will be appended to a's data and b is destroyed
 * */
static int pyke_http_concat_response(Response *a, Response *b)
{

    if(a->offset+b->offset > a->size) {
        int alloc_size = a->offset + b->offset;
        if(a->size==0) {
            a->buffer = (char*)malloc(alloc_size*sizeof(char)); 
        } else {
            a->buffer = (char*)realloc(a->buffer, alloc_size);
        }
    }
    memcpy(a->buffer+a->offset, b->buffer, b->offset);
    a->offset += b->offset;
    if(b->size > 0)
        free(b->buffer);
    return 0;
}
/* 
 * END: UTIL functions
 */


/*
 * internal 
 *
 * MISC request handler; output various error messages
 * */
static int pyke_http_handle_unrecognized(PykeHttp *h, Connection *c, char *status)
{
    char tmpbuf[32] = {0};
    int error;
    int data_length = 0;
    Response headers;
    Response payload;

    memset(&headers, 0, sizeof(Response));
    memset(&payload, 0, sizeof(Response));
    pyke_http_add_response_firstline(&headers, HTTP_VERSION, status);

    pyke_http_add_response_header(&headers, HTTP_HEADER_SERVER, SERVER_NAME);
    pyke_http_add_response_header(&headers, HTTP_HEADER_DATE, mytime_get_formatted_time_str(tmpbuf, sizeof(tmpbuf)));
  
    data_length = pyke_http_add_data(&payload, status, strlen(status));

    snprintf(tmpbuf, sizeof(tmpbuf), "%d", data_length);
    pyke_http_add_response_header(&headers, HTTP_HEADER_CONTENT_LENGTH, tmpbuf);
    pyke_http_add_data(&headers, "\r\n", 2);

    error = pyke_socket_send(&h->s, c, headers.buffer, headers.offset);

    pyke_http_concat_response(&headers, &payload);
    if(headers.size > 0)
        free(headers.buffer);
    return error;
}

/*
 * internal 
 *
 * HEAD request handler
 * */
static int pyke_http_handle_head(PykeHttp *h, Connection *c, HttpRequest *r)
{
    char path[4096] = {0};
    struct stat info;
    char status[128] = {0};
    char tmpbuf[32] = {0};
    int error;
    int data_length = 0;
    Response headers;

    memset(&headers, 0, sizeof(Response));
    snprintf(path, sizeof(path), "%s/%s", h->wwwfolder, r->request_uri);
    
    if((error=stat(path, &info)) < 0) {
        LOGI("Error retrieving file details using stat errno: %d\n", errno);
        switch(errno) {
            case EACCES:
                snprintf(status, sizeof(status), "403 Forbidden");
                break;
            default: 
                snprintf(status, sizeof(status), "404 Not Found");
        }
    }

    //fill response headers and data
    pyke_http_add_response_firstline(&headers, HTTP_VERSION, status);

    pyke_http_add_response_header(&headers, HTTP_HEADER_SERVER, SERVER_NAME);
    pyke_http_add_response_header(&headers, HTTP_HEADER_DATE, mytime_get_formatted_time_str(tmpbuf, sizeof(tmpbuf)));
    pyke_http_add_response_header(&headers, HTTP_HEADER_CONTENT_TYPE, get_mime_from_file_path(path));

    if(error>=0) {
        pyke_http_add_response_header(&headers, HTTP_HEADER_MODIFIED_DATE, 
            mytime_get_formatted_time_from_epoch_time(info.st_mtime, tmpbuf, sizeof(tmpbuf)) );
        data_length = info.st_size;
    }

    snprintf(tmpbuf, sizeof(tmpbuf), "%d", data_length);
    pyke_http_add_response_header(&headers, HTTP_HEADER_CONTENT_LENGTH, tmpbuf);
    pyke_http_add_data(&headers, "\r\n", 2);

    error = pyke_socket_send(&h->s, c, headers.buffer, headers.offset);

    if(headers.size > 0)
        free(headers.buffer);

    return error;
}

/*
 * internal 
 *
 * POST request handler
 * */
static int pyke_http_handle_post(PykeHttp *h, Connection *c, HttpRequest *r, char *post_data)
{
    
    char path[4096] = {0};
    struct stat info;
    char status[128] = {0};
    char tmpbuf[32] = {0};
    int error;
    int data_length = 0;
    Response headers;
    Response payload;

    memset(&headers, 0, sizeof(Response));
    memset(&payload, 0, sizeof(Response));
    snprintf(path, sizeof(path), "%s/%s", h->wwwfolder, r->request_uri);
    
    if((error=stat(path, &info)) < 0) {
        LOGI("Error retrieving file details using stat errno: %d\n", errno);
        switch(errno) {
            case EACCES:
                snprintf(status, sizeof(status), "403 Forbidden");
                break;
            default: 
                snprintf(status, sizeof(status), "404 Not Found");
        }
    }

    LOGI("POSTDATA: %s\n", post_data);

    //fill response headers and data
    pyke_http_add_response_firstline(&headers, HTTP_VERSION, status);

    pyke_http_add_response_header(&headers, HTTP_HEADER_SERVER, SERVER_NAME);
    pyke_http_add_response_header(&headers, HTTP_HEADER_DATE, mytime_get_formatted_time_str(tmpbuf, sizeof(tmpbuf)));
    pyke_http_add_response_header(&headers, HTTP_HEADER_CONTENT_TYPE, get_mime_from_file_path(path));

    if(error>=0) {
        pyke_http_add_response_header(&headers, HTTP_HEADER_MODIFIED_DATE, 
            mytime_get_formatted_time_from_epoch_time(info.st_mtime, tmpbuf, sizeof(tmpbuf)) );
    }

    if(errno==EACCES)
        data_length = pyke_http_add_data(&payload, HTTP_403, strlen(HTTP_403));
    else {
        data_length = pyke_http_add_file_data(&payload, path);
        if(data_length < 0)
            data_length = pyke_http_add_data(&payload, HTTP_404, strlen(HTTP_404));
    }

    snprintf(tmpbuf, sizeof(tmpbuf), "%d", data_length);
    pyke_http_add_response_header(&headers, HTTP_HEADER_CONTENT_LENGTH, tmpbuf);
    pyke_http_add_data(&headers, "\r\n", 2);

    pyke_http_concat_response(&headers, &payload);

    error = pyke_socket_send(&h->s, c, headers.buffer, headers.offset);

    if(headers.size > 0)
        free(headers.buffer);

    return 0;
}

/*
 * internal 
 *
 * GET request handler
 * */
static int pyke_http_handle_get(PykeHttp *h, Connection *c, HttpRequest *r)
{
    char path[4096] = {0};
    struct stat info;
    char status[128] = {0};
    char tmpbuf[32] = {0};
    int error;
    int data_length = 0;
    Response headers;
    Response payload;

    memset(&headers, 0, sizeof(Response));
    memset(&payload, 0, sizeof(Response));
    snprintf(path, sizeof(path), "%s/%s", h->wwwfolder, r->request_uri);
    
    if((error=stat(path, &info)) < 0) {
        LOGI("Error retrieving file details using stat errno: %d\n", errno);
        switch(errno) {
            case EACCES:
                snprintf(status, sizeof(status), "403 Forbidden");
                break;
            default: 
                snprintf(status, sizeof(status), "404 Not Found");
        }
    }

    //fill response headers and data
    pyke_http_add_response_firstline(&headers, HTTP_VERSION, status);

    pyke_http_add_response_header(&headers, HTTP_HEADER_SERVER, SERVER_NAME);
    pyke_http_add_response_header(&headers, HTTP_HEADER_DATE, mytime_get_formatted_time_str(tmpbuf, sizeof(tmpbuf)));
    pyke_http_add_response_header(&headers, HTTP_HEADER_CONTENT_TYPE, get_mime_from_file_path(path));

    if(error>=0) {
        pyke_http_add_response_header(&headers, HTTP_HEADER_MODIFIED_DATE, 
            mytime_get_formatted_time_from_epoch_time(info.st_mtime, tmpbuf, sizeof(tmpbuf)) );
    }

    if(errno==EACCES)
        data_length = pyke_http_add_data(&payload, HTTP_403, strlen(HTTP_403));
    else {
        data_length = pyke_http_add_file_data(&payload, path);
        if(data_length < 0)
            data_length = pyke_http_add_data(&payload, HTTP_404, strlen(HTTP_404));
    }

    snprintf(tmpbuf, sizeof(tmpbuf), "%d", data_length);
    pyke_http_add_response_header(&headers, HTTP_HEADER_CONTENT_LENGTH, tmpbuf);
    pyke_http_add_data(&headers, "\r\n", 2);

    pyke_http_concat_response(&headers, &payload);

    error = pyke_socket_send(&h->s, c, headers.buffer, headers.offset);

    if(strcmp(r->connection, "close") == 0) {
        pyke_socket_client_shutdown(&h->s, c);
    }

    if(headers.size > 0)
        free(headers.buffer);
    return error;
}

/* callback from socket layer
 *
 * return 0 on success when the whole data is received
 *        
 * */
int http_cb_on_data_recv(void *ctxt, Connection *c, char *data, int len)
{
    PykeHttp *h;

    char buffer[MAX_REQUEST_SIZE] = {0};
    char *saveptr = {0};
    char *line_token;
    int method; // 1-get, 2-post, 3-head
    char *request_uri;
    char *connection;
    char *prev_token;
    int content_length = -1;
    int error;
    HttpRequest request;
    PykeRequestHeaders headers;

    memset(&headers, 0, sizeof(PykeRequestHeaders));
    memset(&request, 0, sizeof(HttpRequest));

    h = (PykeHttp*)ctxt;
    memcpy(buffer, data, len);

    //first line of http request check for get/post/head method
    line_token = strtok_r(buffer, HTTP_LINE_DELIM, &saveptr);
    if(line_token == NULL) {
        LOGI("Incomplete request %s\n", line_token);
        //pyke_http_handle_unrecognized(h, c, HTTP_400);
        return -2; 
    }
    LOGI("%s; remote ip: %s\n", line_token, c->client_ip);
    method = pyke_http_parse_first_line(line_token, &request_uri);
    if(method == -2) {
        LOGI("Incomplete request %s\n", line_token);
        //pyke_http_handle_unrecognized(h, c, HTTP_400);
        return -2; 
    }

    while( NULL != (line_token=strtok_r(NULL, HTTP_LINE_DELIM, &saveptr)) ) {

        char *key, *value;
        int i;
        int line_length;

        if(line_token == NULL) {
            pyke_http_handle_unrecognized(h, c, HTTP_400);
            return -1; 
        }
        line_length = strlen(line_token);

        for(i=0; i<line_length; i++) {
            if(line_token[i] == ':')
                break;
        }
        if(i!=line_length) {
            line_token[i] = '\0';
            key = line_token;
            if(i+2 < line_length) //2 because ':' is followed by space
                value = line_token+i+2;

            if(strcmp(key, HTTP_HEADER_CONNECTION) == 0) {
                connection = value;
                headers.HDR_CONNECTION = value;
            } else if(strcmp(key, HTTP_HEADER_CONTENT_LENGTH) == 0) {
                content_length = atoi(value);
                headers.HDR_CONTENT_LENGTH = value;
            } else if(strcmp(key, HTTP_HEADER_CONTENT_TYPE ) == 0) {
                headers.HDR_CONTENT_TYPE = value;
            } else if(strcmp(key, HTTP_HEADER_ACCEPT ) == 0) {
                headers.HDR_ACCEPT = value;
            } else if(strcmp(key, HTTP_HEADER_REFERRER ) == 0) {
                headers.HDR_REFERER = value;
            } else if(strcmp(key, HTTP_HEADER_ACCEPT_ENCODING ) == 0) {
                headers.HDR_ACCEPT_ENCODING = value;
            } else if(strcmp(key, HTTP_HEADER_ACCEPT_LANGUAGE ) == 0) {
                headers.HDR_ACCEPT_LANGUAGE = value;
            } else if(strcmp(key, HTTP_HEADER_ACCEPT_CHARSET ) == 0) {
                headers.HDR_ACCEPT_CHARSET = value;
            } else if(strcmp(key, HTTP_HEADER_HOST ) == 0) {
                headers.HDR_HOST = value;
            } else if(strcmp(key, HTTP_HEADER_COOKIE ) == 0) {
                headers.HDR_COOKIE = value;
            } else if(strcmp(key, HTTP_HEADER_USER_AGENT ) == 0) {
                headers.HDR_USER_AGENT = value;
            }

            line_token[i] = ':';
        }
        prev_token = line_token;
    }

    request.method = method;
    request.request_uri = request_uri;
    request.connection = connection;
    request.content_length = content_length;

    if(method>0 && method<4 && pyke_cgi_handler_is_cgi_request(request_uri)) {
        char *post_data="";
        if(content_length > 0)
            post_data = prev_token;
        error = pyke_cgi_handler_spawn(h, c, &headers, METHODS[method-1], request_uri, post_data); 
    } else if(method == 1) {
        error = pyke_http_handle_get(h, c, &request);
    } else if(method == 2) {
        char *post_data = NULL;
        if(content_length < 0)
            error = pyke_http_handle_unrecognized(h, c, HTTP_411);
        else if(content_length > 0)
            post_data = prev_token; 
        error = pyke_http_handle_post(h, c, &request, post_data);
    }
    else if(method == 3)
        error = pyke_http_handle_head(h, c, &request);
    else if(method == -1) 
        error = pyke_http_handle_unrecognized(h, c, HTTP_505);
    else 
        error = pyke_http_handle_unrecognized(h, c, HTTP_501);

    return error; 
}

int http_cb_on_error(PykeHttp *h, Connection *c, int error)
{
    if(error == 500) {
        pyke_http_handle_unrecognized(h, c, HTTP_500);
    }
    return error;
}
