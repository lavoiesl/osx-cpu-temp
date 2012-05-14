
CC     = cc
CFLAGS = -O2 -Wall
INC    = -framework IOKit
PREFIX = /usr/local
EXEC   = osx-cpu-temp

build : $(EXEC)

clean : 
	rm $(EXEC)

$(EXEC) : smc.c
	$(CC) $(CFLAGS) $(INC) -o $@ $?

install : $(EXEC)
	install $(EXEC) $(PREFIX)/bin