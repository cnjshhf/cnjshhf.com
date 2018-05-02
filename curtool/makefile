.SUFFIXES:.c .o
CC=g++
SRCS=curtool.c
LIBS=qstring.c \
	confparser.c
OBJS=$(SRCS:.c=.o)
LBJS=$(LIBS:.c=.o)
EXEC=curtool
LIBC=libpr.a
start:$(LBJS) $(OBJS)
	ar -rsv $(LIBC) $(LBJS) 
	ar -t $(LIBC)
	$(CC) -o $(EXEC) $(OBJS) -L./ -lpr 
.c.o:
	$(CC) -Wall -g -o $@ -c $<
cl:    
	rm -rf $(EXEC) $(OBJS) $(LBJS) $(LIBC) 
r:
	make cl
	make
rlog:
	rm -rf $(EXEC).log
	touch $(EXEC).log
