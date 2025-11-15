
SHELL = /bin/bash

#	Compiler
CXX = clang++ -c -std=c++23 -fPIC

#	Boost version
BV = 1.87

CXXFLAGS = -Wall -Winvalid-pch -Wno-macro-redefined \
	-I/opt/local/libexec/boost/$(BV)/include

SRCS = $(basename $(wildcard *.cp))
OBJS = $(addsuffix .o, $(SRCS))

all: $(OBJS)
$(OBJS): %.o: %.cp %.h Makefile
	$(CXX) -x c++-header $(CXXFLAGS) $ -O3 $< -o $@

.PHONY: clean all
clean:
	rm -f $(addsuffix .o, $(SRCS))
