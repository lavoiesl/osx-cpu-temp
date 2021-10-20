CC     = clang
CFLAGS = -O3 -Wall
INC    = -framework IOKit
PREFIX = /usr/local
EXEC   = osx-cpu-temp
SOURCES := $(shell find . -name '*.c')

build : $(EXEC)

clean : 
	rm -f $(EXEC) *.o *.out

$(EXEC) : main.cpp smc.o
	$(CC) $(CFLAGS) $(INC) -o $@ $?

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

install : $(EXEC)
	@install -v $(EXEC) $(PREFIX)/bin/$(EXEC)


.PHONY: clean install build
