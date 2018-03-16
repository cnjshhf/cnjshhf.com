.SUFFIXES:.c .o
INCLUDE=-I/usr/local/include/libxml2 -L/usr/local/lib
CC=g++
SRCS=xmltpbus.c                                                                        
OBJS=$(SRCS:.c=.o)
EXEC=xmltpbus
start:$(OBJS) 
	$(CC) -o $(EXEC) $(OBJS) -lxml2  -lz -lm 
.c.o:
	$(CC) -Wall -g -o $@ $(INCLUDE) -c $< 
cl:    
	rm -rf $(EXEC) $(OBJS)
r:
	make cl
	make
