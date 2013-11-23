
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/socket.h>

#include "pyke_ssl.h"
#include "log.h"

int pyke_ssl_init(PykeSSL *s, char *private_key_file, char *public_key_file)
{

    SSL_load_error_strings();
    SSL_library_init();
    if ((s->ctxt = SSL_CTX_new(TLSv1_server_method())) == NULL)
    {
        LOGE("Error creating SSL context.\n");
        return EXIT_FAILURE;
    }


    /* register private key */
    if (SSL_CTX_use_PrivateKey_file(s->ctxt, private_key_file,
                                    SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(s->ctxt);
        LOGE("Error associating private key.\n");
        return EXIT_FAILURE;
    }

    /* register public key (certificate) */
    if (SSL_CTX_use_certificate_file(s->ctxt, public_key_file,
                                     SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(s->ctxt);
        LOGE("Error associating certificate.\n");
        return EXIT_FAILURE;
    }
    return 0;
}

int pyke_ssl_close(PykeSSL *s)
{
    if(s->ctxt!=NULL)
        SSL_CTX_free(s->ctxt);
    return 0;
}

int pyke_ssl_client_init(PykeSSL *s, PykeSSLClient *c, int fd)
{
    memset(c, 0, sizeof(PykeSSLClient));
    c->fd = fd;

    if ((c->ctxt = SSL_new(s->ctxt)) == NULL)
    {
        LOGE("Error creating client SSL context.\n");
        return -1;
    }

    if (SSL_set_fd(c->ctxt, fd) == 0)
    {
        LOGE("Error creating client SSL context.\n");
        return -1;
    }  

    if (SSL_accept(c->ctxt) <= 0)
    {
        LOGE("Error accepting (handshake) client SSL context.\n");
        return -1;
    }
    return 0;
}

int pyke_ssl_client_close(PykeSSLClient *c)
{
    if(c->ctxt==NULL)
        return 0;
    SSL_shutdown(c->ctxt);
    SSL_free(c->ctxt);
    return 0;
}

int pyke_ssl_client_send(PykeSSLClient *c, char *buf, int nbytes)
{
    int nsent = 0, want_write = 0;
    int ret;
    do{
        ret=SSL_write(c->ctxt, buf+nsent, nbytes-nsent);
        if(ret < 0) {
            int error = SSL_get_error(c->ctxt, ret);
            if(error == SSL_ERROR_WANT_WRITE || error == SSL_ERROR_WANT_READ) {
                want_write = 1;
            } else {
                LOGE("Error in SSL_write %d\n", error);
                return -1;
            }
        } else if(ret > 0){
            nsent += ret;
        } else {
            //client shutdown
            return 0;
        }
        
    } while(want_write);

    return 0;
}

int pyke_ssl_client_recv(PykeSSLClient *c, char *buf, int size)
{
    int nread = 0;
    int ret, error;
    int want_read = 0;

    do {
        ret = SSL_read(c->ctxt, buf+nread, size-nread);
        if(ret < 0) {
            error = SSL_get_error(c->ctxt, ret);
            if(error == SSL_ERROR_NONE) {

            }
            else if( error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
                want_read = 1; 
            else {
                LOGE("Error in SSL_read %d\n", error);
                return -1; 
            }
        } else if(ret == 0) {
            //client shutdown
            return 0; 
        } else {
            nread += ret;
        }
    } while(SSL_pending(c->ctxt) || want_read);

    return nread;
}
