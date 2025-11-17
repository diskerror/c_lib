
SHELL = /bin/bash

#	Compiler
CXX = g++ -c -std=c++23 -fPIC

#	Boost version
BV = 1.87

CXXFLAGS = -Wall -Winvalid-pch -Wno-macro-redefined

BOOST = -I/opt/local/libexec/boost/$(BV)/include

BUILD_PREF = build/
LIB_PREf = lib/libdiskerror_

AUDIO_LIB = $(addprefix $(LIB_PREf), audio.a)
#AUDIO = BigFloat80 AudioFile AudioSamples
AUDIO = BigFloat80
AUDIO_OBJS = $(addprefix $(BUILD_PREF), $(addsuffix .o, $(AUDIO)))

#CLAPP_LIB = $(addprefix $(LIB_PREf), clapp.a)
#CLAPP = clapp clappFiles FileList
#CLAPP_OBJS = $(addprefix $(BUILD_PREF), $(addsuffix .o, $(CLAPP)))

.PHONY: clean all

#all: $(AUDIO_LIB) $(CLAPP_LIB)
all: $(AUDIO_LIB)

$(AUDIO_LIB): $(CLAPP_OBJS)
	ar -rc $@ $(AUDIO_OBJS)

#$(CLAPP_LIB): $(CLAPP_OBJS)
#	ar -rc $@ $(CLAPP_OBJS)

$(addprefix $(BUILD_PREF), BigFloat80.o): BigFloat80.cp BigFloat80.h Makefile
	$(CXX) $(CXXFLAGS) $(BOOST) -O3 $< -o $@

#$(addprefix $(BUILD_PREF), AudioFile.o): AudioFile.cp AudioFile.h Makefile
#	$(CXX) $(CXXFLAGS) $(BOOST) -O3 $< -o $@

#$(addprefix $(BUILD_PREF), AudioSamples.o): AudioSamples.cp AudioSamples.h Makefile
#	$(CXX) $(CXXFLAGS) $(BOOST) -O3 $< -o $@

#$(addprefix $(BUILD_PREF), clapp.o): clapp.cp clapp.h Makefile
#	$(CXX) $(CXXFLAGS) -O3 $< -o $@

#$(addprefix $(BUILD_PREF), clappFiles.o): clappFiles.cp clappFiles.h Makefile
#	$(CXX) $(CXXFLAGS) -O3 $< -o $@

#$(addprefix $(BUILD_PREF), FileList.o): FileList.cp FileList.h Makefile
#	$(CXX) $(CXXFLAGS) $(BOOST) -O3 $< -o $@


#clean:
#	rm -f $(addsuffix .a, $(SRCS))
