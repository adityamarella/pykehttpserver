
**Pyke HTTP Server**

Implementation:

Checkpoint 1:

Event driven programming is used to wait on multiple IO operations concurrently. After creating the server socket and binding to the given port, 'select' system call is called with server socket file descriptor added to the readfds.

New client connection:
 - new Connection object is created and appended to the list of connection objects(maintained as a doubly linked list) this is to store the number of bytes read and echoed back
 - client fd is added to the readfds, so that select can notify any incoming data on the client fd

Receiving data(client fd is active):
 - data is read and stored in the buffer corresponding to the Connection object
 - client fd is added to the list of writefds, so that the select can notify when the client fd is ready to be written
 - if zero bytes are sent then that means client has closed the connection. So the connection object is cleared and client fd is closed

Sending data:
 - If data sent is equal to data recevied then client fd is removed from the writefds and the connection state is cleared
 - Otherwise, Connection object state is updated and control is given back to select

Checkpoint 2:
Http layer is added on top of the socket server implemented in check point 1. HEAD, GET, POST are supported as of now; 200, 404, 400, 403, 501, 500, 503, 505 status codes are supported. 

Checkpoint 3:
SSL and CGI support are added to the existing server implementation. If the url contains "/cgi/" CGI script is invoked, the script path is taken from the command line. The CGI script is forked and an IPC channel is established between the CGI and PYKE server. After 

Files:
file names are self explanatory
- pyke_main.c is the entry point
- socket layer has pyke_socket<>.x files
- http layer has pyke_http<>.x
- logger is log.c log.h 
- pyke_time.c/h is a util file

Compiling:
#> make

Running:
#> Usage: ./pyked <HTTP port> <HTTPS port> <log ﬁle> <lock file> <www folder>

