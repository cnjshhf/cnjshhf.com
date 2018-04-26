.SUFFIXES:.c .o
CC=gcc
SRCS=main.c\
	des.c                                                                        
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
