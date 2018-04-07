.SUFFIXES:.c .o
CC=g++
SRCS=epoll.c
OBJS=$(SRCS:.c=.o)
EXEC=epoll
start:$(OBJS)  
	$(CC) -o $(EXEC) $(OBJS)
.c.o:
	$(CC) -Wall -g -o $@ -c $<
cl:    
	rm -rf $(EXEC) $(OBJS)
r:
	make cl
	make
