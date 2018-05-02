.SUFFIXES:.c .o
CC=g++
SRCS=mulfork.c
OBJS=$(SRCS:.c=.o)
EXEC=exec
start:$(OBJS) $(LBJS) 
	$(CC) -o $(EXEC) $(OBJS) -lpthread 
.c.o:
	$(CC) -Wall -g -o $@ -c $< 
cl:    
	rm -rf $(EXEC) $(OBJS)
r:
	make cl
	make
