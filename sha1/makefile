.SUFFIXES:.c .o
CC=gcc
SRCS=sha1.c                                                                        
OBJS=$(SRCS:.c=.o)
EXEC=sha1
start:$(OBJS) 
	$(CC) -o $(EXEC) $(OBJS)  
.c.o:
	$(CC) -Wall -g -o $@  -c $< 
cl:    
	rm -rf $(EXEC) $(OBJS)
r:
	make cl
	make
