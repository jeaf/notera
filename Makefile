CFLAGS   = -Wall -std=c99
CPPFLAGS = -Wall -std=c++0x
EXEC     = main.exe

OBJS           = main.o sha1.o sqlite3.o
SQLITE_FLAGS   = -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_TEMP_STORE=3
CFLAGS        += $(SQLITE_FLAGS)
CPPFLAGS      += $(SQLITE_FLAGS)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+

main.o: main.cpp sqlite3.h
sha1.o: sha1.c sha1.h
sqlite3.o: sqlite3.c sqlite3.h

.PHONY: clean
clean:
	rm -f $(EXEC) $(OBJS)

