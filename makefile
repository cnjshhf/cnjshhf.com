.SUFFIXES:.c .o
CC=g++
INCLUDE=-I/usr/local/include/libxml2 -L/usr/local/lib
SRCS=tool.c
LIBS=qstring.c \
	confparser.c \
	dso.c
DLLS=xml.c
DLLO=xml.so
DLLS1=mytest.c
DLLO1=mytest.so
DLLS2=test.c
DLLO2=test.so
OBJS=$(SRCS:.c=.o)
LBJS=$(LIBS:.c=.o)
EXEC=tool
LIBC=libpr.a
start:$(LBJS) $(OBJS)
	ar -rsv $(LIBC) $(LBJS) 
	ar -t $(LIBC)
	$(CC) -o $(EXEC) $(OBJS) -L./ -lpr -ldl 
.c.o:
	$(CC) -Wall -g -o $@ $(INCLUDE) -c $< 
cl:    
	rm -rf $(EXEC) $(OBJS) $(LBJS) $(LIBC) $(DLLO) $(DLLO1) $(DLLO2) 
dll:
	$(CC) -O -fpic -shared -o $(DLLO) $(INCLUDE) $(DLLS) -lxml2  -lz -lm
	$(CC) -O -fpic -shared -o $(DLLO1)  $(INCLUDE) $(DLLS1) -lxml2  -lz -lm
	$(CC) -O -fpic -shared -o $(DLLO2)  $(INCLUDE) $(DLLS2) -lxml2  -lz -lm
r:
	make cl
	make
	make dll
rlog:
	rm -rf $(EXEC).log
	touch $(EXEC).log
