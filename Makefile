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

OBJS         = autolink.o buffer.o houdini_href_e.o houdini_html_e.o html.o main.o markdown.o sha1.o sqlite3.o stack.o
SQLITE_FLAGS = -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_TEMP_STORE=3
CFLAGS      += $(SQLITE_FLAGS)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+

autolink.o: autolink.c autolink.h
buffer.o: buffer.c buffer.h
houdini_href_e.o: houdini_href_e.c
houdini_html_e.o: houdini_html_e.c
html.o: html.c html.h
html.o: html.c html.h
main.o: main.c sqlite3.h
markdown.o: markdown.c markdown.h
sha1.o: sha1.c sha1.h
sqlite3.o: sqlite3.c sqlite3.h
stack.o: stack.c stack.h

.PHONY: clean
clean:
	$(RM) -f $(EXEC) $(OBJS)

