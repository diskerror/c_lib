
SHELL = /bin/bash

#	Compiler
CP = clang++ -std=c++23

#	Boost version
BV = 1.81

CX = $(CP) -Wall -Winvalid-pch \
	-I/opt/local/libexec/boost/$(BV)/include

# XCODE=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers

SRCS=$(basename $(wildcard *.cp))
OBJS=$(addsuffix .o, $(SRCS))

all: $(OBJS)
$(OBJS): %.o: %.cp %.h Makefile
	$(CX) -O2 -x c++-header $< -o $@

.PHONY: clean all
clean:
	rm -f $(addsuffix .o, $(SRCS))
