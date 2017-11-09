CFLAGS	= -Wall -Wextra -pedantic -fPIC

PREFIX	= /usr/local
LIBDIR	= $(PREFIX)/lib
INCDIR	= $(PREFIX)/include
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/man/man3

HDRS	= audio.h
LIBS	= libaudio.a libaudio.so
OBJS	= audio.o pcm.o
MAN3	= libaudio.3
TEST	= test-file test-rw

all: $(LIBS)

libaudio.a: $(OBJS)
	ar -r libaudio.a $(OBJS)

libaudio.so: $(OBJS)
	$(CC) -shared -o libaudio.so $(OBJS)

audio.o: $(HDRS) audio.c pcm.h
	$(CC) $(CFLAGS) -c audio.c

pcm.o: $(HDRS) pcm.c pcm.h
	$(CC) $(CFLAGS) -c pcm.c

lint: $(MAN3)
	mandoc -Tlint -Wstyle $(MAN3)

install: $(LIBS) $(HDRS) $(MAN3) lint
	install -d $(LIBDIR)
	install -d $(INCDIR)
	install -d $(MANDIR)
	install $(LIBS) $(LIBDIR)
	install $(HDRS) $(INCDIR)
	install $(MAN3) $(MANDIR)

test: $(TEST)
	./test-file     2> /dev/null
	./test-rw -l 5  2> /dev/null

test-file: test-file.c $(LIBS) $(HDRS)
	$(CC) $(CFLAGS) -o test-file test-file.c libaudio.a

test-rw: test-rw.c $(LIBS) $(HDRS)
	$(CC) $(CFLAGS) -lm -o test-rw test-rw.c libaudio.a

uninstall:
	cd $(LIBDIR) && rm -f $(LIBS)
	cd $(INCDIR) && rm -f $(HDRS)
	cd $(MANDIR) && rm -f $(MAN3)

clean:
	rm -f $(LIBS) $(OBJS) $(TEST)
	rm -f *.raw *.core *~
