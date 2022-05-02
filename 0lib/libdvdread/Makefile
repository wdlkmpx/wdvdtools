include ../../config.mk

LIB_STATIC = libdvdread.a

# ls *.c | sed "s%\.c%.o%g" | tr "\n" " " >> Makefile
OBJS = bitreader.o dvd_input.o dvd_reader.o dvd_udf.o ifo_print.o ifo_read.o logger.o md5.o nav_print.o nav_read.o 

#CPPFLAGS += -I..
CFLAGS += -I../.. -I.
LIBS   += 

all: $(LIB_STATIC)

$(LIB_STATIC): $(OBJS)
	$(AR) rcs $(LIB_STATIC) $(OBJS)

strip:

distclean: clean

clean:
	rm -f *.o *~ *.a *.so

install:

install-strip: install

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(LIB_STATIC)*
