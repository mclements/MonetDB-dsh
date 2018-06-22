name     = MonetDB-dsh
version  = `sed -n 's/^Version:[ \t]*\(.*\)/\1/p' MonetDB-dsh.spec`

CC       = cc
DBFARM   = ~/work/mydbfarm

LIBDIR   = $(shell pkg-config --variable=libdir monetdb5)
# CFLAGS  += -g -Wall
CFLAGS  += $(shell pkg-config --cflags monetdb5)
LDFLAGS += $(shell pkg-config --libs monetdb5)

all: lib_dsh.so

lib_dsh.so: dsh.o dsh.mal 75_dsh.sql
	$(CC) -fPIC -DPIC -o lib_dsh.so -shared dsh.o $(LDFLAGS) -Wl,-soname -Wl,lib_dsh.so

dsh.o: dsh.c
	$(CC) -fPIC -DPIC $(CFLAGS) -c dsh.c

clean:
	rm -f *.o *.so

install: lib_dsh.so
	mkdir -p $(DESTDIR)$(LIBDIR)/monetdb5/autoload $(DESTDIR)$(LIBDIR)/monetdb5/createdb
	cp dsh.mal lib_dsh.so $(DESTDIR)$(LIBDIR)/monetdb5
	cp 75_dsh.sql $(DESTDIR)$(LIBDIR)/monetdb5/createdb
	cp 75_dsh.mal $(DESTDIR)$(LIBDIR)/monetdb5/autoload

uninstall:
	rm $(DESTDIR)$(LIBDIR)/monetdb5/dsh.mal || true
	rm $(DESTDIR)$(LIBDIR)/monetdb5/lib_dsh.so || true
	rm $(DESTDIR)$(LIBDIR)/monetdb5/createdb/75_dsh.sql || true
	rm $(DESTDIR)$(LIBDIR)/monetdb5/autoload/75_dsh.mal || true

redo: clean lib_dsh.so install test

dist:
	tar -c -j -f $(name)-$(version).tar.bz2 --transform "s,^,$(name)-$(version)/," `hg files -X .hgtags`

sql:
	monetdbd start $(DBFARM) || true
	mclient -d testt

mal:
	monetdbd start $(DBFARM) || true
	mclient -l mal testt

start:
	monetdbd start $(DBFARM) || true

stop:
	monetdbd stop $(DBFARM) || true

test:
	monetdbd start $(DBFARM) || true
	monetdb destroy -f testt || true
	monetdb create testt
	monetdb release testt
	monetdb set embedr=yes testt
	monetdb set embedc=yes testt
	mclient -l mal -d testt < test.mal
