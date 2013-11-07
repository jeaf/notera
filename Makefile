ifneq ($(findstring .exe, $(SHELL)), )
    CC     = tcc
    CFLAGS = -Wall -Werror
    EXEC   = main.exe
    RM     = rm.exe
else
    CFLAGS = -Wall -Werror -std=c99
    EXEC   = main.cgi
    RM     = rm
endif

OBJS         = main.o sha1.o sqlite3.o
SQLITE_FLAGS = -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_TEMP_STORE=3
CFLAGS      += $(SQLITE_FLAGS)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+

main.o: main.c sqlite3.h
sha1.o: sha1.c sha1.h
sqlite3.o: sqlite3.c sqlite3.h

.PHONY: clean
clean:
	$(RM) -f $(EXEC) $(OBJS)

