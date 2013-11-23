#ifndef __PYKE_SSL_H__
#define __PYKE_SSL_H__

#include <openssl/ssl.h>

typedef struct {

    SSL_CTX *ctxt;

}PykeSSL;

typedef struct {

    int fd;
    char buffer[8192];
    SSL *ctxt;

} PykeSSLClient;

int pyke_ssl_init(PykeSSL *s, char *private_key_file, char *public_key_file);

int pyke_ssl_close(PykeSSL *s);

int pyke_ssl_client_init(PykeSSL *s, PykeSSLClient *c, int fd);

int pyke_ssl_client_close(PykeSSLClient *c);

int pyke_ssl_client_send(PykeSSLClient *c, char *buf, int nbytes);

int pyke_ssl_client_recv(PykeSSLClient *c, char *buf, int size);

#endif
