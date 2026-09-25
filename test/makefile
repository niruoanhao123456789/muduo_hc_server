all: poller
poller: pollertest.cpp ../source/Server.hpp $(wildcard ../log/*.hpp)
	g++ -std=c++20 $< -o $@


.PHONY:
clean:
	rm -f poller