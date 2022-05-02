include ../config.mk

APP  = dvdregionset
# ls *.c | sed "s%\.c%.o%g" | tr "\n" " " >> Makefile
OBJS = dvd_udf.o regionset.o 

#CPPFLAGS += -I..
CFLAGS += -I..

CFLAGS += $(LIBDVDREAD_CFLAGS)
LIBS   += $(LIBDVDREAD_LIBS)

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(APP) $(LDFLAGS) $(LIBS)

strip: all
	@if test -f $(APP).exe ; then \
		echo $(STRIP) $(APP).exe; \
		$(STRIP) $(APP).exe; \
	else \
		echo $(STRIP) $(APP); \
		$(STRIP) $(APP); \
	fi;

distclean: clean

clean:
	rm -f *.o *~ $(APP) $(APP).exe

install: all
	mkdir -p $(DESTDIR)$(bindir)
	@if test -f $(APP).exe ; then \
		echo install $(APP).exe; \
		install $(APP).exe $(DESTDIR)$(bindir); \
	else \
		echo install $(APP); \
		install $(APP) $(DESTDIR)$(bindir); \
	fi;

install-strip: strip install

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(APP)
