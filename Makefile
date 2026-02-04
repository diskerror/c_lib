
SHELL = /bin/bash

#	Compiler
#	On MacOS g++: aliased to /opt/local/bin/g++-mp-15
CXX = g++ -c -std=c++23 -fPIC -Wall -Wextra -Winvalid-pch -Wno-macro-redefined

CXXFLAGS = -I/opt/local/libexec/gcc15/libc++/include -I/opt/local/libexec/boost/1.87/include

BUILD_PREF = build/
LIB_PREF = lib/libdiskerror_

AUDIO_LIB = $(addprefix $(LIB_PREF), audio.a)
AUDIO = AudioFile AudioFormat AudioSamples
AUDIO_OBJS = $(addprefix $(BUILD_PREF), $(addsuffix .o, $(AUDIO)))

#CLAPP_LIB = $(addprefix $(LIB_PREF), clapp.a)
#CLAPP = clapp clappFiles FileList
#CLAPP_OBJS = $(addprefix $(BUILD_PREF), $(addsuffix .o, $(CLAPP)))

LINK = g++ -std=c++23 -Wall -Wextra -Wno-macro-redefined

.PHONY: clean all test

#all: $(AUDIO_LIB) $(CLAPP_LIB)
all: $(AUDIO_LIB)

$(AUDIO_LIB): $(addprefix $(BUILD_PREF), $(addsuffix .o, $(AUDIO)))
	ar -rc $@ $^

#$(CLAPP_LIB): $(CLAPP_OBJS)
#	ar -rc $@ $(CLAPP_OBJS)

$(addprefix $(BUILD_PREF), AudioFile.o): AudioFile.cp AudioFile.h AudioTypes.h Makefile
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

$(addprefix $(BUILD_PREF), AudioFormat.o): AudioFormat.cp AudioFormat.h AudioFile.h AudioTypes.h BigFloat80.h WAVE.h AIFF.h Makefile
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

$(addprefix $(BUILD_PREF), AudioSamples.o): AudioSamples.cp AudioSamples.h Makefile
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

tests/test_audioformat: tests/test_audioformat.cp $(AUDIO_LIB) AudioFile.h AudioFormat.h AudioTypes.h Makefile
	$(LINK) $(CXXFLAGS) -O0 -g $< -Llib -ldiskerror_audio -o $@

test: tests/test_audioformat
	./tests/test_audioformat

#$(addprefix $(BUILD_PREF), clapp.o): clapp.cp clapp.h Makefile
#	$(CXX)-O3 $< -o $@

#$(addprefix $(BUILD_PREF), clappFiles.o): clappFiles.cp clappFiles.h Makefile
#	$(CXX) -O3 $< -o $@

#$(addprefix $(BUILD_PREF), FileList.o): FileList.cp FileList.h Makefile
#	$(CXX) -O3 $< -o $@


#clean:
#	rm -f $(addsuffix .a, $(SRCS))
