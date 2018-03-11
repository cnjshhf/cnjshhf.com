# Makefile
include $(HOME)/etc/mak.$(OS_TYPE)

AR   = ar
#CC   = cc -brtl
ESQL = proc

MQINCL = -I$(MQM_HOME)/include 
MQLIBS = -L$(MQM_HOME)/lib -lmq -levent

#CFLAGS  = -g -c 
INCLUDE = -I$(TPBUS_HOME)/include $(MQINCL) -I$(TPBUS_HOME)/include/libxml2 -I$(TPBUS_HOME)/include/dbproc/

SYSLIBS = -lm -lxml2 -lpthread $(LIBICONV)

#libsqlite3.a 不能绝对路径
LIBS = -L$(TPBUS_HOME)/lib -ltpbus -lsqlite3 $(MQLIBS) $(SYSLIBS) 
#LIBS = -L$(TPBUS_HOME)/lib -ltpbus $(MQLIBS) $(SYSLIBS) -lsqlite3
#LIBS = -L$(TPBUS_HOME)/lib -ltpbus $(MQLIBS) $(SYSLIBS) 
#LIBSO = -L$(TPBUS_HOME)/lib/ -l libsqlite3.a $(MQLIBS) $(SYSLIBS)
LIBSO = -L$(TPBUS_HOME)/lib -lsqlite3 $(MQLIBS) $(SYSLIBS)
#LIBSO =  $(MQLIBS) $(SYSLIBS) -lsqlite3

.SUFFIXES:	.c .o
.c.o:
		$(CC) -o $*.o $(CFLAGS) $*.c $(INCLUDE)

all: lib bin
lib: $(TPBUS_HOME)/lib/libtpbus.so $(TPBUS_HOME)/lib/libtpCustom.so
bin:	$(TPBUS_HOME)/bin/tpbus	\
	$(TPBUS_HOME)/bin/tpTaskMng	\
	$(TPBUS_HOME)/bin/tpServer	\
	$(TPBUS_HOME)/bin/tpTranMng	\
	$(TPBUS_HOME)/bin/tpAdaptChg	\
	$(TPBUS_HOME)/bin/tpSysMon	\
	$(TPBUS_HOME)/bin/tpAdaptBridge \
	$(TPBUS_HOME)/bin/tpAdaptMonC	\
	$(TPBUS_HOME)/bin/TcpS	\
	$(TPBUS_HOME)/bin/TcpALS \
	$(TPBUS_HOME)/bin/TcpALC \
	$(TPBUS_HOME)/bin/tpAdaptTcpS \
	$(TPBUS_HOME)/bin/tpAdaptTcpC \
	$(TPBUS_HOME)/bin/tpAdaptTcpALS \
	$(TPBUS_HOME)/bin/tpAdaptTcpALC \
	$(TPBUS_HOME)/bin/tpAdaptChg1 \
	$(TPBUS_HOME)/bin/tpAdaptTcpS1 \
	$(TPBUS_HOME)/bin/tpAdaptTcpALS1 \
	$(TPBUS_HOME)/bin/tpAdaptTcpALC1 \
	$(TPBUS_HOME)/bin/tpAdaptMonC1  \
	$(TPBUS_HOME)/bin/tpAdaptTcpC_B64  \
	$(TPBUS_HOME)/bin/tpAdaptTcpS_B64  \
	$(TPBUS_HOME)/bin/tpAdaptDist \
	$(TPBUS_HOME)/bin/tpAdaptChg_pay \
	$(TPBUS_HOME)/bin/tpAdaptChg4   \
	$(TPBUS_HOME)/bin/tpAdaptChg5  \
	$(TPBUS_HOME)/bin/tpAdaptTcpS_q
#	$(TPBUS_HOME)/bin/tpAdaptDist4


$(TPBUS_HOME)/lib/libtpbus.so: sql.o xml.o tpFmtConv.o tpPublic.o tpFunExec.o tpThread.o  log.o 
	rm -f $@
	$(CC) $(SHAREDFLAGS) -o $@ $?  $(LIBSO)

$(TPBUS_HOME)/lib/libtpCustom.so: tpCustom.o xml.o   
	rm -f $@
	$(CC) $(SHAREDFLAGS) -o $@ $?   $(LIBSO)

$(TPBUS_HOME)/bin/tpbus: tpbus.o
	$(CC) -o $@ tpbus.o $(LIBS)

$(TPBUS_HOME)/bin/tpTaskMng: tpTaskMng.o
	$(CC) -o $@ tpTaskMng.o $(LIBS)

$(TPBUS_HOME)/bin/tpServer: tpServer.o
	$(CC) -o $@ tpServer.o $(LIBS)

$(TPBUS_HOME)/bin/tpTranMng: tpTranMng.o
	$(CC) -o $@ tpTranMng.o $(LIBS)

$(TPBUS_HOME)/bin/tpSysMon: tpSysMon.o
	$(CC) -o $@ tpSysMon.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptChg: tpAdaptChg.o
	$(CC) -o $@ tpAdaptChg.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptIcmsChg: tpAdaptIcmsChg.o
	$(CC) -o $@ tpAdaptIcmsChg.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptChg1: tpAdaptChg1.o
	$(CC) -o $@ tpAdaptChg1.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptDist: tpAdaptDist.o
	$(CC) -o $@ tpAdaptDist.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptDist2: tpAdaptDist2.o
	$(CC) -o $@ tpAdaptDist2.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptDist3: tpAdaptDist3.o
	$(CC) -o $@ tpAdaptDist3.o $(LIBS) -L$(TPBUS_HOME)/lib  -ldbopr

$(TPBUS_HOME)/bin/tpAdaptTcpS: tpAdaptTcpS.o
	$(CC) -o $@ tpAdaptTcpS.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpS1: tpAdaptTcpS1.o
	$(CC) -o $@ tpAdaptTcpS1.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpS_B64: tpAdaptTcpS_B64.o
	$(CC) -o $@ tpAdaptTcpS_B64.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpC: tpAdaptTcpC.o
	$(CC) -o $@ tpAdaptTcpC.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpC_B64: tpAdaptTcpC_B64.o
	$(CC) -o $@ tpAdaptTcpC_B64.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpALS: tpAdaptTcpALS.o
	$(CC) -o $@ tpAdaptTcpALS.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpALSyy: tpAdaptTcpALSyy.o
	$(CC) -o $@ tpAdaptTcpALSyy.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpALS1: tpAdaptTcpALS1.o
	$(CC) -o $@ tpAdaptTcpALS1.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpALC: tpAdaptTcpALC.o
	$(CC) -o $@ tpAdaptTcpALC.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpALC1: tpAdaptTcpALC1.o
	$(CC) -o $@ tpAdaptTcpALC1.o $(LIBS)

$(TPBUS_HOME)/bin/tpBizProc: tpBizProc.o
	$(CC) -o $@ tpBizProc.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptMonC: tpAdaptMonC.o
	$(CC) -o $@ tpAdaptMonC.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptMonC1: tpAdaptMonC1.o
	$(CC) -o $@ tpAdaptMonC1.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptBridge: tpAdaptBridge.o
	$(CC) -o $@ tpAdaptBridge.o $(LIBS)

$(TPBUS_HOME)/bin/TcpALS: TcpALS.o
	$(CC) -o $@ TcpALS.o $(LIBS)

$(TPBUS_HOME)/bin/TcpALC: TcpALC.o
	$(CC) -o $@ TcpALC.o $(LIBS)

$(TPBUS_HOME)/bin/tpFlapTcpALS: tpFlapTcpALS.o
	$(CC) -o $@ tpFlapTcpALS.o $(LIBS)

$(TPBUS_HOME)/bin/tpFlapTcpALC: tpFlapTcpALC.o
	$(CC) -o $@ tpFlapTcpALC.o $(LIBS)

$(TPBUS_HOME)/bin/TcpS: TcpS.o
	$(CC) -o $@ TcpS.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptChg2: tpAdaptChg2.o
	$(CC) -o $@ tpAdaptChg2.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptChg_pay: tpAdaptChg_pay.o
	$(CC) -o $@ tpAdaptChg_pay.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptPollChg: tpAdaptPollChg.o
	$(CC) -o $@ tpAdaptPollChg.o $(LIBS)
    
$(TPBUS_HOME)/bin/tpAdaptChg4: tpAdaptChg4.o
	$(CC) -o $@ tpAdaptChg4.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptTcpS_q: tpAdaptTcpS_q.o
	$(CC) -o $@ tpAdaptTcpS_q.o $(LIBS)

$(TPBUS_HOME)/bin/tpAdaptChg5: tpAdaptChg5.o
	$(CC) -o $@ tpAdaptChg5.o $(LIBS)

#$(TPBUS_HOME)/bin/tpAdaptDist4: tpAdaptDist4.o
#	$(CC) -o $@ tpAdaptDist4.o $(LIBS)

clean:
	rm -f *.o
