all: server client
server: servertest.cpp ../source/Server.hpp $(wildcard ../log/*.hpp)
	g++ -std=c++20 $< -o $@

client: clienttest.cpp ../source/Server.hpp $(wildcard ../log/*.hpp)
	g++ -std=c++20 $< -o $@

.PHONY:
clean:
	rm -f server client