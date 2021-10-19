# Makefile for the cgi programs,
# you must copy the pbl src directory *.c and *.h files into this directory before you can make
#
STRIP=   /usr/bin/strip
AR=      /usr/bin/ar
RANLIB=  /usr/bin/ar ts
IPATH=   -I.
#CFLAGS=  -Wall -Wno-unused-but-set-variable -O3 -std=c99 ${IPATH}
CFLAGS=  -Wall -O3 -std=c99 ${IPATH}
CC= gcc

INCLIB    = 

LIB_OBJS  = pblCgi.o pblStringBuilder.o pblPriorityQueue.o pblHeap.o pblMap.o pblSet.o pblList.o pblCollection.o pblIterator.o pblhash.o pbl.o
THELIB    = libpbl.a

EXE_OBJS1 = LendMeYourFace-m.o
THEEXE1   = LendMeYourFace-m.cgi

all: $(THELIB) $(THEEXE1) $(THEEXE2)

$(THELIB):  $(LIB_OBJS)
	$(AR) rc $(THELIB) $?
	$(RANLIB) $(THELIB)

$(THEEXE1):  $(EXE_OBJS1) $(THELIB)
	$(CC) -O3 -lm -o $(THEEXE1) $(EXE_OBJS1) $(THELIB) $(INCLIB)
	$(STRIP) $(THEEXE1)
	
clean:
	rm -f ${THELIB}  ${LIB_OBJS} core
	rm -f ${THEEXE1} ${EXE_OBJS1}
