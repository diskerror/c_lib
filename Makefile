
SHELL = /bin/bash

XCODE=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers

SRCS=$(basename $(wildcard *.cp))
OBJS=$(addsuffix .o, $(SRCS))

all: $(OBJS)
$(OBJS): %.o: %.cp %.h Makefile
	g++ -Wall -Winvalid-pch -c -I/opt/local/include -I$(XCODE) -O2 -x c++-header $< -o $@

.PHONY: clean
clean:
	rm -f $(addsuffix .o, $(SRCS))
