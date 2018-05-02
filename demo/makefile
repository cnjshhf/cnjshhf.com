.SUFFIXES:.c .o
CC=g++
SRCS=confparser.c
LIBS=qstring.c
OBJS=$(SRCS:.c=.o)
LBJS=$(LIBS:.c=.o)
EXEC=exec
LIBC=libpr.a
start:$(OBJS) $(LBJS) 
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
