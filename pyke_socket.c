#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "log.h"
#include "pyke_socket.h"
#include "pyke_http_cb.h"

#define BACKLOGS 20

int pyke_socket_init(PykeSocket *s, void *ctxt, int server_port, int ssl_port, char *private_key_file, char *public_key_file)
{
    memset(s, 0, sizeof(PykeSocket));
    s->port = server_port;
    s->ssl_port = ssl_port;
    s->ctxt = ctxt;

    pyke_ssl_init(&s->ssl, private_key_file, public_key_file);
    return 0;
}

int pyke_socket_close(PykeSocket *s)
{
    delall_connection(&s->head);
    close(s->sockfd);
    close(s->ssl_sockfd);
    pyke_ssl_close(&s->ssl);
    return 0;
}

int pyke_socket_client_shutdown(PykeSocket *s, Connection *c)
{
    FD_CLR(c->fd, &s->write_fds);
    FD_CLR(c->fd, &s->read_fds);
    if(c->is_ssl)
        pyke_ssl_client_close(&c->ctxt);
    close(c->fd);
    del_connection_object(c);

    return 0;
}

int pyke_socket_send(PykeSocket *s, Connection *c, char *data, int len)
{
    int ret = 0;

    if(c->is_ssl)
        ret = pyke_ssl_client_send(&c->ctxt, data, len);
    else
        ret = send(c->fd, data, len, 0);

    if(ret < 0) {
        LOGE("Error send failed %d\n", errno);
        if(errno==ECONNRESET) {
            pyke_socket_client_shutdown(s, c);
        }
    } else {
        c->size = 0;
        write(c->replayfd, data, len);
    }
    
    return ret;
}

int pyke_socket_cgi_pipe_read(PykeSocket *s, Connection *c) 
{
    int ret;
    char buf[MAX_REQUEST_SIZE];

    ret = read(c->pipe_fd, buf, sizeof(buf));
    
    if(ret<0) {
        http_cb_on_error(s, c, 500);
        return -1;
    }

    ret = pyke_socket_send(s, c, buf, ret);
    if(ret>0) {
        LOGI("Response sent to: %s\n", c->client_ip);
    }
    return 0;
}

int pyke_socket_recv(PykeSocket *s, Connection *c)
{
    // read data sent by client 'ifd'
    int ret;
    char buf[MAX_REQUEST_SIZE] = {0};
    int fd;

    fd = c->fd;

    if(c->is_ssl) {
        ret = pyke_ssl_client_recv(&c->ctxt, buf, sizeof(buf));
    } else {
        ret = recv(fd, buf, sizeof(buf), 0);
    }

    if(ret > 0) {
        if(c->size + ret <= (int)sizeof(c->buffer)) {
            memcpy(c->buffer+c->size, buf, ret);
            c->size += ret;
            http_cb_on_data_recv(s->ctxt, c, c->buffer, c->size);
        }
    } else {
        if(ret < 0)
            LOGE("recv error from client fd: %d errno: %d\n", fd, errno);
        pyke_socket_client_shutdown(s, c); 
    }
    return ret;
}

int pyke_socket_add_read_fd(PykeSocket *s, int fd)
{
    FD_SET(fd, &s->read_fds);
    s->fdmax = s->fdmax>fd?s->fdmax:fd;
    return 0; 
}

int pyke_socket_clr_read_fd(PykeSocket *s, int fd) 
{
    FD_CLR(fd, &s->read_fds);
    return 0;
}

static int pyke_socket_create_bind_listen(struct sockaddr_in *addr, int server_port) 
{
    int error, flag = 1;
    int sockfd; 

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        LOGE("socket creation failed \n");
        deinit_log();
        return 1;
    }

    error = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag,
            sizeof(int));
    if(error == -1) {
        LOGE("setsockopt failed \n");
        return 1;
    }

    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(server_port);
    addr->sin_addr.s_addr = INADDR_ANY;

    error = bind(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
    if(error == -1) {
        LOGE("Error bind failed for port %d\n", server_port);
        return -1;
    }

    error = listen(sockfd, SOMAXCONN);
    if(error == -1) {
        LOGE("listen failed\n");
        return 1; 
    }
    return sockfd;
}

/*
 * initializes the select server
 *
 * @server_port server port
 * */
int pyke_socket_run_server(PykeSocket *s)
{
    struct sockaddr_in addr;
    struct sockaddr_in client_addr; // client address
    char client_ip[INET6_ADDRSTRLEN];
    int error;
    fd_set tmp1, tmp2;
    int server_port = s->port;
   

    s->sockfd = pyke_socket_create_bind_listen(&addr, s->port);
    s->ssl_sockfd = pyke_socket_create_bind_listen(&addr, s->ssl_port);

    LOGI("Server Started(fd %d). Listening on port %d\n", s->sockfd, server_port);

    memset(&s->head, 0, sizeof(Connection));
    
    FD_ZERO(&s->read_fds);
    FD_ZERO(&s->write_fds);
    FD_SET(s->sockfd, &s->read_fds);
    FD_SET(s->ssl_sockfd, &s->read_fds);

    
    while(1) {

        int ifd;
        tmp1 = s->read_fds; //select makes changes to these so working on a copy
        tmp2 = s->write_fds;

        s->fdmax = 0;
        s->fdmax = s->sockfd > s->ssl_sockfd?s->sockfd:s->ssl_sockfd;

        //reset fdmax
        Connection *iter;
        for(iter=s->head.next; iter!=NULL; iter=iter->next) {
            s->fdmax = iter->fd > s->fdmax? iter->fd:s->fdmax;
            if(iter->is_cgi_request)
                s->fdmax = iter->pipe_fd > s->fdmax? iter->pipe_fd:s->fdmax;
        }

        error = select(s->fdmax+1, &tmp1, &tmp2, NULL, 0);
        if(error == -1) {
            LOGE("select returned error: %d\n", errno);
            return -1;
        }

        if(FD_ISSET(s->sockfd, &tmp1)) {
            // server socket is active so new client connection
            int client_fd;
            socklen_t addrlen = 0;

            memset(&client_addr, 0, sizeof client_addr);
            memset(client_ip, 0, sizeof client_ip);
            addrlen = sizeof(client_addr);

            client_fd = accept(s->sockfd, (struct sockaddr*)&client_addr, &addrlen);
            if(client_fd == -1) {
                LOGE("accept failed\n"); 
            } else {
                FD_SET(client_fd, &s->read_fds);
                s->fdmax = client_fd > s->fdmax? client_fd:s->fdmax;
                if(NULL == inet_ntop(client_addr.sin_family, &((&client_addr)->sin_addr),
                            client_ip, INET6_ADDRSTRLEN))
                    LOGE("Error getting client IP; errno=%d\n", errno);
                add_connection(&s->head, NULL, client_fd, client_ip, 0);
            }
        } 

        if(FD_ISSET(s->ssl_sockfd, &tmp1)) {
            // server socket is active so new client connection
            int client_fd;
            socklen_t addrlen = 0;

            memset(&client_addr, 0, sizeof client_addr);
            memset(client_ip, 0, sizeof client_ip);
            addrlen = sizeof(client_addr);

            client_fd = accept(s->ssl_sockfd, (struct sockaddr*)&client_addr, &addrlen);
            if(client_fd == -1) {
                LOGE("accept failed\n"); 
            } else {
                FD_SET(client_fd, &s->read_fds);
                s->fdmax = client_fd > s->fdmax? client_fd:s->fdmax;
                if(NULL == inet_ntop(client_addr.sin_family, &((&client_addr)->sin_addr),
                            client_ip, INET6_ADDRSTRLEN))
                    LOGE("Error getting client IP; errno=%d\n", errno);

                PykeSSLClient *client_ctxt = NULL;

                /*client ctxt has to be maintained in the connection linked list*/
                Connection *c = add_connection(&s->head, &client_ctxt, client_fd, client_ip, 1);
                error = pyke_ssl_client_init(&s->ssl, client_ctxt, client_fd);
                if(error == -1) {
                    pyke_socket_client_shutdown(s, c);
                }

            }
        }
        Connection *it = NULL;

        for(it=s->head.next; it!=NULL; it=it->next) {
            int pipe_fd = it->pipe_fd;
            ifd = it->fd;

            //readfds
            if(FD_ISSET(ifd, &tmp1)) {
                pyke_socket_recv(s, it);
                break;
            }

            if(it->is_cgi_request) {
                if(FD_ISSET(pipe_fd, &tmp1)) {
                    LOGI("CGI response received. Sending the response to %s\n", it->client_ip);
                    pyke_socket_cgi_pipe_read(s, it); 
                    FD_CLR(pipe_fd, &s->read_fds);
                    close(pipe_fd);
                    it->is_cgi_request = 0;
                    it->pipe_fd = 0;
                    break;
                }  
            }

            //writefds
            if(FD_ISSET(ifd, &tmp2)) {
                break;
            }
        }

    }
    return 0;
}
