
SHELL = /bin/bash

#	Compiler
CXX = clang++ -c -std=c++23 -fPIC -Wall -Wextra -Winvalid-pch

CXXFLAGS = -Wno-multichar \
	-I/opt/local/libexec/gcc15/libc++/include \
	-I/opt/local/libexec/boost/1.88/include

BUILD_PREF = build/
LIB_PREF = lib/libdiskerror_

AUDIO_LIB = $(addprefix $(LIB_PREF), audio.a)
AUDIO = AudioFile AudioFormat AudioSamples

OPTIONS_LIB = $(addprefix $(LIB_PREF), options.a)
OPTIONS = ProgramOptions

LINK = clang++ -std=c++23 -Wall -Wextra -Wno-macro-redefined

.PHONY: clean all test

all: $(AUDIO_LIB) $(OPTIONS_LIB)

$(AUDIO_LIB): $(addprefix $(BUILD_PREF), $(addsuffix .o, $(AUDIO)))
	ar -rc $@ $^

$(addprefix $(BUILD_PREF), AudioFile.o): AudioFile.cp AudioFile.h AudioTypes.h Makefile
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

$(addprefix $(BUILD_PREF), AudioFormat.o): AudioFormat.cp AudioFormat.h AudioFile.h AudioTypes.h BigFloat80.h WAVE.h AIFF.h Makefile
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

$(addprefix $(BUILD_PREF), AudioSamples.o): AudioSamples.cp AudioSamples.h Makefile
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

$(OPTIONS_LIB): $(addprefix $(BUILD_PREF), $(addsuffix .o, $(OPTIONS)))
	ar -rc $@ $^

$(addprefix $(BUILD_PREF), ProgramOptions.o): ProgramOptions.cp ProgramOptions.h Makefile
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

tests/test_audioformat: tests/test_audioformat.cp $(AUDIO_LIB) AudioFile.h AudioFormat.h AudioTypes.h Makefile
	$(LINK) $(CXXFLAGS) -O0 -g $< -Llib -ldiskerror_audio -o $@

tests/test_audiosamples: tests/test_audiosamples.cp $(AUDIO_LIB) AudioFile.h AudioFormat.h AudioTypes.h Makefile
	$(LINK) $(CXXFLAGS) -O0 -g $< -Llib -ldiskerror_audio -o $@

test: tests/test_audioformat tests/test_audiosamples
	./tests/test_audioformat
	./tests/test_audiosamples

clean:
	rm -f ./build/*.o ./lib/*.a
