CFLAGS   = -std=c99
CXXFLAGS = -std=c++0x -I.
CPPFLAGS = -O0
EXEC     = cgi-bin/api

OBJS           = api.o db.o sha1.o sqlite3.o sqlite_wrapper.o util.o
SQLITE_FLAGS   = -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_TEMP_STORE=3
CFLAGS        += $(SQLITE_FLAGS)
CXXFLAGS      += $(SQLITE_FLAGS)

$(EXEC).exe: $(OBJS)
	g++ $(CXXFLAGS) -o $@ $+

api.o: api.cpp sha1.h sqlite_wrapper.h util.h
db.o: db.cpp db.h sqlite_wrapper.h
sha1.o: sha1.cpp sha1.h
sqlite3.o: sqlite3.c sqlite3.h
sqlite_wrapper.o: sqlite_wrapper.cpp sqlite_wrapper.h sqlite3.h util.h
util.o: util.cpp util.h

.PHONY: clean
clean:
	rm -f $(EXEC).exe $(OBJS)

