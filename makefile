.SUFFIXES:.c .o
CC=gcc
SRCS=conf2xml.c
LIBS=findtask.c\
	findmapp.c
OBJS=$(SRCS:.c=.o)
LBJS=$(LIBS:.c=.o)
EXEC=conf2xml
LIBC=libpr.a
start:$(LBJS) $(OBJS) 
	ar -rsv $(LIBC) $(LBJS)
	ar -t $(LIBC)	
	$(CC) -o $(EXEC) $(OBJS) -L./ -lpr
.c.o:
	$(CC) -Wall -g -o $@ -c $< --std=c99 
cl:			
	rm -rf $(EXEC) $(OBJS) $(LBJS) $(LIBC)
r:
	make cl
	make
