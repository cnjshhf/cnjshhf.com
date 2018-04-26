.SUFFIXES:.c .o
CC=gcc
SRCS=base64.c                                                                        
OBJS=$(SRCS:.c=.o)
EXEC=main
start:$(OBJS) 
	$(CC) -o $(EXEC) $(OBJS)  
.c.o:
	$(CC) -Wall -g -o $@  -c $< 
cl:    
	rm -rf $(EXEC) $(OBJS)
r:
	make cl
	make
