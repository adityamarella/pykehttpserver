/*
 * Connection state implemented as a linked list
 * 
 * */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "pyke_socket_connection.h"
#include "log.h"

Connection* add_connection(Connection *head, PykeSSLClient **client, int fd, char *client_ip, int is_ssl)
{
    Connection *t, *iter;
    char buf[64];

    t = (Connection*)malloc(sizeof(Connection));
    memset(t, 0, sizeof(Connection));
    t->fd = fd;
    t->sent = 0;
    t->is_ssl = is_ssl;

    for(iter=head; iter->next!=NULL; iter=iter->next);
        
    iter->next = t;
    t->prev = iter;

    snprintf(buf, sizeof(buf), "replay_%d.out", fd);
    t->replayfd = open(buf, O_CREAT|O_WRONLY|O_APPEND, S_IRWXU);

    if(client != NULL)
        *client = &t->ctxt;

    snprintf(t->client_ip, sizeof(t->client_ip), "%s", client_ip);

    return t;
}

int del_connection_object(Connection *c) 
{
    if(c == NULL)
        return 0;

    if(c->prev!=NULL)
        c->prev->next = c->next;

    if(c->next!=NULL)
        c->next->prev = c->prev;

    close(c->replayfd);
    free(c);

    return 0;
}

int del_connection(Connection *head, int fd)
{
    Connection *iter = NULL;

    for(iter=head; iter!=NULL; iter=iter->next)
        if(iter->fd == fd)
            break;
    
    if(iter == NULL)
       return 0;

    if(iter->prev != NULL)
        iter->prev->next = iter->next;

    if(iter->next!=NULL)
        iter->next->prev = iter->prev;

    close(iter->replayfd);
    free(iter);

    return 0;
}

int delall_connection(Connection *c)
{
    Connection *iter = NULL;

    for(iter=c->next; iter!=NULL;) {
        Connection *curr;
        curr = iter;
        iter = iter->next;
        close(curr->replayfd);
        free(curr);
    }
    c->next = NULL;
    return 0;
}


