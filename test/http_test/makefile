test : httpclienttest.cc
	g++ -std=c++20 $^ -o $@

perf : httpperftest.cc
	g++ -std=c++20 -O2 -pthread $^ -o $@

.PHONY: clean
clean:
	rm -f test perf
