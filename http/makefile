server : main.cc $(wildcard *.hpp) $(wildcard ../source/*.hpp) $(wildcard ../log/*.hpp)
	g++ -std=c++20 $< -o $@

.PHONY: clean
clean:
	rm -f server
