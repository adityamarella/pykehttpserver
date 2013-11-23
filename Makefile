
CC = gcc
CFLAGS = -Wall -Werror -Wextra -Wshadow -Wunreachable-code -D__DEBUG
LDFLAGS = -lssl -lcrypt

UTIL_SRCS = log.c pyke_time.c
UTIL_OBJS = log.o pyke_time.o
PYKE_SRCS = pyke_main.c pyke_http.c pyke_cgi_handler.c pyke_socket.c pyke_socket_connection.c pyke_ssl.c
PYKE_OBJS = pyke_main.o pyke_http.o pyke_cgi_handler.o pyke_socket.o pyke_socket_connection.o pyke_ssl.o

SRCS = $(UTIL_SRCS) $(PYKE_SRCS)
OBJS = $(UTIL_OBJS) $(PYKE_OBJS) 

pyke_main: pyke_main.o util.o
	        $(CC) -o pyked $(PYKE_OBJS) $(UTIL_OBJS) $(LDFLAGS)

util.o: $(UTIL_SRCS)
	 		$(CC) $(CFLAGS) -c $(UTIL_SRCS)

pyke_main.o: $(PYKE_SRCS)
	        $(CC) $(CFLAGS) -c $(PYKE_SRCS)

clean:
	        rm -f *.o pyked replay_* pyke.log /tmp/lock
