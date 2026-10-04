TARGETS = echoserver echoclient

all : $(TARGETS)

echoserver : echoserver.cc
	g++ -std=c++20 $^ -o $@

echoclient : echoclinet.cc
	g++ -std=c++20 $^ -o $@

.PHONY:
clean:
	rm -f $(TARGETS)