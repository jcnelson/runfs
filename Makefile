CC    := cc
CFLAGS := -std=c11 -Wall -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
LIB   := -lfuse -lpthread -lrt -lfskit -lfskit_fuse -lpstat
INC   := -I. 
C_SRCS:= $(wildcard *.c)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

RUNFS := runfs

DESTDIR ?= /
PREFIX ?= /usr
BINDIR ?= $(DESTDIR)/$(PREFIX)/bin

all: runfs

runfs: $(OBJ)
	$(CC) $(CFLAGS) -o $(RUNFS) $(OBJ) $(LIBINC) $(LIB)

install: runfs
	mkdir -p $(BINDIR)
	cp -a $(RUNFS) $(BINDIR)

%.o : %.c
	$(CC) $(CFLAGS) -o "$@" $(INC) -c "$<" $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(RUNFS)
