CC=g++
CFLAGS=-Wall -std=c++11 -g -O3
LDFLAGS=
LDLIBS=-l boost_program_options -l boost_filesystem -lboost_system
SOURCES = src/*.cc
HEADERS = src/*.hh
BINFILE = 

# default
.PHONY: all
all: alpine_scheduler

alpine_scheduler: $(SOURCES) $(HEADERS) makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o alpine_scheduler $(SOURCES) $(LDLIBS)

.PHONY: clean 
clean:
	rm -f alpine_scheduler
