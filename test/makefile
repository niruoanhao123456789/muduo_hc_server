CXX      := g++
CXXFLAGS := -std=c++20 -pthread
DEPS     := $(wildcard ../source/*.hpp) $(wildcard ../log/*.hpp)

BINS := servertest clienttest eventlooptest channeltest pollertest \
        sockettest acceptortest connectiontest anytest tcpservertest

all: $(BINS)

%: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) $< -o $@


.PHONY: all clean
clean:
	rm -f $(BINS)
