
CC     = cc
CFLAGS = -O2 -Wall
INC    = -framework IOKit
PREFIX = /usr/local
EXEC   = osx-cpu-temp
SOURCES := $(shell find . -name '*.c')

build : $(EXEC)

clean : 
	rm $(EXEC)

$(EXEC) : smc.c
	$(CC) $(CFLAGS) $(INC) -o $@ $?

install : $(EXEC)
	@install -v $(EXEC) $(PREFIX)/bin/$(EXEC)

lint: bin/lint.sh $(SOURCES)
	@$<

lint-fix: bin/lint-fix.sh $(SOURCES)
	@$<

.PHONY: lint lint-fix clean install build
