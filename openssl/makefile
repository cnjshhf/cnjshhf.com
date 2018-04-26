.SUFFIXES:.c .o
CC=gcc
SRCS=dsa.c
OBJS=$(SRCS:.c=.o)
EXEC=main
start:$(OBJS) 
	$(CC) -o $(EXEC) $(OBJS) -lssl -lcrypto
.c.o:
	$(CC) -Wall -g -o $@  -c $< 
cl:    
	rm -rf $(EXEC) $(OBJS)
r:
	make cl
	make
