#!/bin/sh

if [ ! -f misc/dvdread.pc.in ] ; then
    echo "  run this script inside the libdvdread source tree"
    echo "  it will create libdvdread/ to be used by W projects.."
    exit 1
fi

mkdir -p libdvdread

cp -av src/* libdvdread
cp -av msvc/contrib/win32_cs.h libdvdread/

patch libdvdread/dvd_input.c <<EOF
--- dvd_input.c	2022-06-01 21:57:43.000000000 +0800
+++ dvd_input-patched.c	2022-06-03 15:21:57.000000000 +0800
@@ -37,6 +37,12 @@
 #include "dvd_input.h"
 #include "logger.h"
 
+#ifdef __GNUC__
+#define UNUSED __attribute__((unused))
+#else
+#define UNUSED
+#endif
+
 
 /* The function pointers that is the exported interface of this file. */
 dvd_input_t (*dvdinput_open)  (void *, dvd_logger_cb *,
@@ -60,11 +66,6 @@
 /* dlopening libdvdcss */
 # if defined(HAVE_DLFCN_H) && !defined(USING_BUILTIN_DLFCN)
 #  include <dlfcn.h>
-# else
-#   if defined(_WIN32)
-/* Only needed on MINGW at the moment */
-#    include "../msvc/contrib/dlfcn.c"
-#   endif
 # endif
 
 typedef struct dvdcss_s *dvdcss_t;
@@ -337,7 +338,7 @@
   /* linking to libdvdcss */
   dvdcss_library = &dvdcss_library;  /* Give it some value != NULL */
 
-#else
+#elif defined(HAVE_DLFCN_H)
   /* dlopening libdvdcss */
 
 #ifdef __APPLE__
EOF

#-----

sed -i 's%\.\./msvc/contrib/%%' libdvdread/*.c
sed -i 's%msvc/contrib/%%' libdvdread/*.c

