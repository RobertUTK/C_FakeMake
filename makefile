CC = gcc
INCLUDES = -I/home/jplank/cs360/include
CFLAGS = -Wall -g -O $(INCLUDES)
LIBDIR = /home/jplank/cs360/lib
LIBS = $(LIBDIR)/libfdr.a

EXECUTABLES = fakemake

all: $(EXECUTABLES)

fakemake: src/fakemake.c
	$(CC) $(CFLAGS) -o fakemake src/fakemake.c $(LIBS)

clean:
	rm -f $(EXECUTABLES) *.o
