include ../config.mk

APP  = subtitle2vobsub

#CPPFLAGS += -I..
CFLAGS += -I..

#LIBS    := -lm 
#DEFINES  += -DHAVE_LIB_PPM_
#LIBS     += -lppm
#DEFINES += -DHAVE_PNG_
#LIBS    += -lpng
#DEFINES += -DHAVE_ZLIB_
#LIBS    += -lz

CFLAGS += 
LIBS   += -lm

all: subtitle2pgm subtitle2vobsub srttool vobsub2pgm

subtitle2pgm: subtitle2pgm.o spudec.o
	$(CC) $(CFLAGS) subtitle2pgm.o spudec.o -o subtitle2pgm $(LDFLAGS) $(LIBS)

subtitle2vobsub: subtitle2vobsub.o vobsub.o 
	$(CC) $(CFLAGS) subtitle2vobsub.o vobsub.o -o subtitle2vobsub $(LDFLAGS) $(LIBS)

srttool: srttool.o
	$(CC) $(CFLAGS) srttool.o -o srttool $(LDFLAGS) $(LIBS)

vobsub2pgm: vobsub2pgm.o vobsub.o spudec.o
	$(CC) $(CFLAGS) vobsub2pgm.o vobsub.o spudec.o -o vobsub2pgm $(LDFLAGS) $(LIBS)

strip: all
	@if test -f subtitle2vobsub.exe ; then \
		$(STRIP) subtitle2pgm.exe subtitle2vobsub.exe srttool.exe vobsub2pgm.exe; \
	else \
		$(STRIP) subtitle2pgm subtitle2vobsub srttool vobsub2pgm; \
	fi;

distclean: clean

clean:
	rm -f *.o *~ subtitle2pgm subtitle2vobsub srttool vobsub2pgm \
	subtitle2pgm.exe subtitle2vobsub.exe srttool.exe vobsub2pgm.exe

install: $(APP)
	mkdir -p $(DESTDIR)$(bindir)
	install -c $(APP) $(DESTDIR)$(bindir)

install-strip: strip install

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(APP)

