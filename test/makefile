# Makefile for creating tests for ModImaging.
CC=gcc
CFLAGS=-Wall -O2 -msse2 -c $(shell GraphicsMagick-config --cflags --cppflags) -I../src/
LDFLAGS=$(shell GraphicsMagick-config --libs) -lcrypto

all: benchmark test

benchmark: benchmark.o imaging.o
	@echo Building benchmark
	$(CC) benchmark.o imaging.o -o "benchmark" $(LDFLAGS)
test: test.o imaging.o
	@echo Building test
	$(CC) test.o imaging.o -o "test" $(LDFLAGS)
benchmark.o:
	@echo Compiling benchmark.c
	$(CC) $(CFLAGS) benchmark.c
test.o: test.c
	@echo Compiling test.c
	$(CC) $(CFLAGS) test.c
imaging.o: ../src/imaging.h ../src/imaging.c
	@echo Compiling imaging.c
	$(CC) $(CFLAGS) ../src/imaging.c
clean:
	@echo Removing object files and test program.
	rm *.o
	rm test benchmark


