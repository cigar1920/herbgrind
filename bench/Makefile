CFLAGS=-g -std=c11 -lm
CAMLFLAGs=

CSRC = $(wildcard *.c)
COUT = $(patsubst %.c,%.c.out,$(CSRC))
CPPSRC = $(wildcard *.cpp)
CPPOUT = $(patsubst %.cpp,%.cpp.out,$(CPPSRC))
CAMLSRC = $(wildcard *.ml)
CAMLOUT = $(patsubst %.ml,%.ml.out,$(CAMLSRC))

CC ?= gcc
CXX ?= g++
CAMLC ?= ocamlopt -g

# NOTE : does not build mini-nocall.sout (trouble on osx)
all: $(COUT) $(CAMLOUT) $(CPPOUT)

testprog.c.out: testprog.c
	$(CC) -o $@ $< $(CFLAGS) -lmpfr
	chmod u+x $@

%.c.out: %.c
	$(CC) -o $@ $< $(CFLAGS)
	chmod u+x $@
%.cpp.out: %.cpp
	$(CXX) -o $@ $< $(CFLAGS)
	chmod u+x $@

%.ml.out: %.ml
	$(CAMLC) -o $@ $< $(CAMLFLAGS)
	chmod u+x $@

%.sout: %.s
	$(CC) -o $@ $< $(CFLAGS)
	chmod u+x $@

%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<



clean:
	rm -rf *.out *.sout *.dSYM
