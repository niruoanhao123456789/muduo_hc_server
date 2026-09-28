CXX      := g++
CXXFLAGS := -std=c++20 -pthread
DEPS     := $(wildcard ../source/*.hpp) $(wildcard ../log/*.hpp)

all: servertest clienttest

servertest: servertest.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) $< -o $@

clienttest: clienttest.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) $< -o $@


.PHONY: all clean
clean:
	rm -f servertest clienttest
