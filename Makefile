CFLAGS   = -std=c99
CXXFLAGS = -std=c++0x -I.
CPPFLAGS = -O3
EXEC     = main.exe

OBJS           = main.o sha1.o sqlite3.o
SQLITE_FLAGS   = -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_TEMP_STORE=3
CFLAGS        += $(SQLITE_FLAGS)
CXXFLAGS      += $(SQLITE_FLAGS)

$(EXEC): $(OBJS)
	g++ $(CXXFLAGS) -o $@ $+

main.o: main.cpp sha1.h sqlite3.h
sha1.o: sha1.cpp sha1.h
sqlite3.o: sqlite3.c sqlite3.h

.PHONY: clean
clean:
	rm -f $(EXEC) $(OBJS)

