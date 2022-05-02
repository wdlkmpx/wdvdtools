include ../config.mk

APP  = vamps
VAMPS_SRC = requant.c vamps.c

#CPPFLAGS += -I..
CFLAGS += -I..

CFLAGS += $(LIBDVDREAD_CFLAGS)
LIBS   += $(LIBDVDREAD_LIBS) -lm -lpthread

all: $(APP) play_cell

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(VAMPS_SRC) -o $(APP) $(LDFLAGS) $(LIBS)

play_cell:
	$(CC) $(CFLAGS) play_cell.c -o play_cell $(LDFLAGS) $(LIBS)

strip: all
	@if test -f $(APP).exe ; then \
		echo $(STRIP) $(APP).exe; \
		$(STRIP) $(APP).exe play_cell.exe; \
	else \
		echo $(STRIP) $(APP); \
		$(STRIP) $(APP) play_cell; \
	fi;

distclean: clean

clean:
	rm -f *.o *~ $(APP) $(APP).exe play_cell play_cell.exe

install: all
	mkdir -p $(DESTDIR)$(bindir)
	@if test -f $(APP).exe ; then \
		echo install $(APP).exe; \
		install $(APP).exe $(DESTDIR)$(bindir); \
		install play_cell.exe $(DESTDIR)$(bindir); \
	else \
		echo install $(APP); \
		install $(APP) $(DESTDIR)$(bindir); \
		install play_cell $(DESTDIR)$(bindir); \
	fi;

install-strip: strip install

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(APP)

