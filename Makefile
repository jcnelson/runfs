CPP    := g++ -Wall -g
LIB   := -lfuse -lpthread -lrt -lfskit -lfskit_fuse -lpstat
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

RUNFS := runfs

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

all: runfs

runfs: $(OBJ)
	$(CPP) -o $(RUNFS) $(OBJ) $(LIBINC) $(LIB)

install: runfs
	mkdir -p $(BINDIR)
	cp -a $(RUNFS) $(BINDIR)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(RUNFS)
