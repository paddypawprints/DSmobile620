CXX := clang++
# Command Line Tools currently resolve `macosx` to the 27 SDK, which this clang cannot link.
SYSROOT := /Library/Developer/CommandLineTools/SDKs/MacOSX26.sdk
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -isysroot $(SYSROOT) -I protocol/include -I app
BUILD := build

.PHONY: all test clean

all: $(BUILD)/inquiry_test $(BUILD)/ds620ctl

test: $(BUILD)/inquiry_test
	$(BUILD)/inquiry_test

$(BUILD):
	mkdir -p $(BUILD)

PROTOCOL_SRC := protocol/src/scanner.cpp protocol/src/simulated_transport.cpp

$(BUILD)/inquiry_test: protocol/tests/inquiry_test.cpp $(PROTOCOL_SRC) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ protocol/tests/inquiry_test.cpp $(PROTOCOL_SRC)

$(BUILD)/ds620ctl: app/main.cpp app/dext_transport.cpp app/activate.mm $(PROTOCOL_SRC) | $(BUILD)
	$(CXX) $(CXXFLAGS) -fobjc-arc -o $@ app/main.cpp app/dext_transport.cpp app/activate.mm $(PROTOCOL_SRC) -framework IOKit -framework CoreFoundation -framework Foundation -framework SystemExtensions

clean:
	rm -rf $(BUILD)
