test: serverbuffertest.cpp ../source/Buffer.hpp $(wildcard ../log/*.hpp)
	g++ -std=c++20 $< -o $@

.PHONY:
clean:
	rm -f test test_asan test_dbg