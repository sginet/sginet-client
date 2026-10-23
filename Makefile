# sginet Makefile - works with IRIX make/smake and GNU make.
#
# IRIX 6.x / MIPSpro (default):
#   sh third_party/build-ncurses.sh
#   make
#
# IRIX 5.x / IDO cc (C89, o32):
#   sh third_party/build-ncurses.sh
#   smake            (detects IRIX 5.x, same as "smake irix5")
#
# Linux / gcc (for automated testing without IRIX host, builds sginet-linux):
#  !! for development only !!
#   PLATFORM=linux CC=gcc CFLAGS=-O2 sh third_party/build-ncurses.sh
#   make linux
#
# xsginet (X11/Motif client, see x11-client/), needs no ncurses:
#   make xsginet           IRIX 6.x / MIPSpro
#   smake xsginet-irix5    IRIX 5.x / IDO cc
#   make xsginet-linux     Linux / gcc + Motif dev package (development only)
#
# xsginet-globe (OpenGL globe viewer, see x11-globe/), Motif + GLwMDrawingArea, no GLUT:
#   make xsginet-globe           IRIX 6.x / MIPSpro
#   smake xsginet-globe-irix5    IRIX 5.x / IDO cc
#   make xsginet-globe-linux     Linux / gcc + Motif, Mesa GL/GLU and libGLw dev packages
#
# The code must stay C89 (plus long long) for IRIX 5, check on Linux with:
#   make c89check
#
# Library builds go to build-$(PLATFORM)/, so IRIX and Linux builds can share
# a source tree. Object files in the top directory are IRIX objects; run
# "make clean clean-deps" when switching between IRIX 6.5 and IRIX 5 builds.

PLATFORM = irix
CC = c99
OPT = -n32 -mips3 -O2
# MIPSpro remarks that are just noise:
# 1174 declared but never referenced, 1209 controlling expression is constant,
# 1552 set but never used
WARN = -woff 1174,1209,1552

# BearSSL can't detect IRIX by itself
BR_PLATFORM = -DBR_USE_UNIX_TIME=1 -DBR_USE_URANDOM=1 -DBR_64=1
SYSLIBS = -lpthread
AR = ar
ARFLAGS = rc

# IRIX 5 build: compat headers (stdint.h, stdbool.h, snprintf) and defines
DEFS =
COMPAT_INC =
COMPAT_OBJ =

TP = third_party
BUILD = build-$(PLATFORM)
NCURSES_PREFIX = $(BUILD)/ncurses
NCURSES_LIBS = $(NCURSES_PREFIX)/lib/libform.a $(NCURSES_PREFIX)/lib/libmenu.a \
	$(NCURSES_PREFIX)/lib/libpanel.a $(NCURSES_PREFIX)/lib/libncurses.a

BEARSSL_LIB = $(BUILD)/libbearssl.a
JSONC_LIB = $(BUILD)/libjson-c.a

INCLUDES = -I$(NCURSES_PREFIX)/include -I$(TP)/bearssl/inc -I$(TP)
CFLAGS = $(OPT) $(WARN) $(DEFS) $(COMPAT_INC) $(INCLUDES)

SRC = sginet.c cloudAPI.c hwinfo_sgi.c nc_register.c nc_mainmenu.c nc_tools.c str_tools.c \
	nc_listsystems.c nc_softwaresearch.c nc_modelselection.c nc_showmessage.c fileutils.c \
	systemadvisor.c nc_systemadvisor.c nc_screenshot.c nc_sysload.c nc_sgug_forum_news.c \
	nc_shoutbox.c webserver.c http_client.c ta_builtin.c

OBJ = sginet.o cloudAPI.o hwinfo_sgi.o nc_register.o nc_mainmenu.o nc_tools.o str_tools.o \
	nc_listsystems.o nc_softwaresearch.o nc_modelselection.o nc_showmessage.o fileutils.o \
	systemadvisor.o nc_systemadvisor.o nc_screenshot.o nc_sysload.o nc_sgug_forum_news.o \
	nc_shoutbox.o webserver.o http_client.o ta_builtin.o

# xsginet: the Motif client and the shared code it uses
X11_SRC = x11-client/main.c x11-client/w_about.c x11-client/w_updater.c x11-client/w_softwaresearch.c \
	x11-client/w_systeminfo.c x11-client/w_shoutbox.c x11-client/w_screenshot.c x11-client/w_systemadvisor.c
X11_SHARED = cloudAPI.c hwinfo_sgi.c fileutils.c str_tools.c systemadvisor.c http_client.c ta_builtin.c
X11LIBS = -lXm -lXt -lX11
# IRIX 5.3 Motif 1.2 may also need -lPW
X11LIBS_IRIX5 = -lXm -lXt -lX11
X11_TARGET = xsginet
# IRIX 6.5: rld must use the IRIX system Motif/X11/GL, not /usr/sgug/lib32
# from LD_LIBRARY*_PATH in an SGUG-RSE sgugshell
X11_LDFLAGS = -Wl,-no_library_replacement

# xsginet-globe: the OpenGL globe viewer and the same shared code as xsginet
GLOBE_SRC = x11-globe/main.c x11-globe/gl_utility.c x11-globe/notifications.c x11-globe/markers.c \
	x11-globe/mapshapes.c x11-globe/textures.c
GLOBE_LIBS = -lGLw -lGLU -lGL -lXm -lXt -lX11 -lm
GLOBE_LIBS_IRIX5 = -lGLw -lGLU -lGL -lXm -lXt -lX11 -lm
GLOBE_INC =
GLOBE_LDFLAGS =
GLOBE_TARGET = xsginet-globe

JSONC_SRC = arraylist.c debug.c json_c_version.c json_object.c json_object_iterator.c \
	json_pointer.c json_tokener.c json_util.c json_visit.c linkhash.c printbuf.c \
	random_seed.c strerror_override.c

.SUFFIXES: .c .o

# plain "smake" picks the IRIX 5 build on IRIX 5.x
all:
	@if uname -r | grep '^5\.' >/dev/null; then $(MAKE) irix5; else $(MAKE) sginet; fi

sginet: $(OBJ) $(COMPAT_OBJ) $(BEARSSL_LIB) $(JSONC_LIB)
	$(CC) $(OPT) $(OBJ) $(COMPAT_OBJ) -o sginet $(NCURSES_LIBS) $(JSONC_LIB) $(BEARSSL_LIB) $(SYSLIBS)

# IRIX 5.x: IDO cc is C89 (+ long long), no pthreads, no snprintf
IRIX5_VARS = CC=cc OPT=-O2 WARN= DEFS="-DIRIX5 -Dinline=" COMPAT_INC=-Icompat/irix5 \
	COMPAT_OBJ=compat/irix5/snprintf.o SYSLIBS= \
	BR_PLATFORM="-DBR_USE_UNIX_TIME=1 -DBR_USE_URANDOM=1 -DBR_64=0"
irix5:
	$(MAKE) $(IRIX5_VARS) sginet

# xsginet: objects go to $(BUILD)/x11 (no incremental build), separate from the CLI objects
xsginet: $(BEARSSL_LIB) $(JSONC_LIB) $(COMPAT_OBJ)
	@test -d $(BUILD) || mkdir $(BUILD)
	@test -d $(BUILD)/x11 || mkdir $(BUILD)/x11
	@for f in $(X11_SRC) $(X11_SHARED); do \
		echo "$(CC) $$f"; \
		$(CC) $(OPT) $(WARN) $(DEFS) $(COMPAT_INC) -I$(TP)/bearssl/inc -I$(TP) \
			-c $$f -o $(BUILD)/x11/`basename $$f .c`.o || exit 1; \
	done
	$(CC) $(OPT) $(BUILD)/x11/*.o $(COMPAT_OBJ) -o $(X11_TARGET) $(X11_LDFLAGS) $(JSONC_LIB) $(BEARSSL_LIB) $(X11LIBS)

xsginet-irix5:
	$(MAKE) $(IRIX5_VARS) X11LIBS="$(X11LIBS_IRIX5)" X11_LDFLAGS= xsginet

# xsginet-globe: objects go to $(BUILD)/globe (no incremental build)
xsginet-globe: $(BEARSSL_LIB) $(JSONC_LIB) $(COMPAT_OBJ)
	@test -d $(BUILD) || mkdir $(BUILD)
	@test -d $(BUILD)/globe || mkdir $(BUILD)/globe
	@for f in $(GLOBE_SRC) $(X11_SHARED); do \
		echo "$(CC) $$f"; \
		$(CC) $(OPT) $(WARN) $(DEFS) $(COMPAT_INC) $(GLOBE_INC) -I$(TP)/bearssl/inc -I$(TP) \
			-c $$f -o $(BUILD)/globe/`basename $$f .c`.o || exit 1; \
	done
	$(CC) $(OPT) $(BUILD)/globe/*.o $(COMPAT_OBJ) -o $(GLOBE_TARGET) $(X11_LDFLAGS) $(GLOBE_LDFLAGS) $(JSONC_LIB) $(BEARSSL_LIB) $(GLOBE_LIBS)

xsginet-globe-irix5:
	$(MAKE) $(IRIX5_VARS) GLOBE_LIBS="$(GLOBE_LIBS_IRIX5)" X11_LDFLAGS= xsginet-globe

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@


NCURSES_STAMP = $(NCURSES_PREFIX)/lib/libncurses.a

$(OBJ): $(NCURSES_STAMP)

# No per-file dependency tracking: rebuild all CLI objects on IRIX when any header changes,
# otherwise a changed struct leaves stale objects
HDRS = include/cloudAPI.h include/fileutils.h include/http_client.h include/hwinfo_sgi.h \
	include/nc_listsystems.h include/nc_mainmenu.h include/nc_modelselection.h include/nc_register.h \
	include/nc_screenshot.h include/nc_sgug_forum_news.h include/nc_shoutbox.h include/nc_showmessage.h \
	include/nc_softwaresearch.h include/nc_sysload.h include/nc_systemadvisor.h include/nc_tools.h \
	include/sginet.h include/str_tools.h include/systemadvisor.h include/webserver.h

$(OBJ): $(HDRS)

$(NCURSES_STAMP):
	@echo "ncurses for $(PLATFORM) not built yet, run: PLATFORM=$(PLATFORM) sh third_party/build-ncurses.sh"
	@false

# Vendored libraries: rebuilt as a whole when the archive is missing (make clean-deps)
# (find needs -print: old find on IRIX 5 prints nothing without it)
$(BEARSSL_LIB):
	@echo "Building BearSSL..."
	@test -d $(BUILD) || mkdir $(BUILD)
	@test -d $(BUILD)/bearssl || mkdir $(BUILD)/bearssl
	@for f in `find $(TP)/bearssl/src -name '*.c' -print`; do \
		$(CC) $(OPT) $(WARN) $(DEFS) $(COMPAT_INC) $(BR_PLATFORM) -I$(TP)/bearssl/inc -I$(TP)/bearssl/src \
			-c $$f -o $(BUILD)/bearssl/`basename $$f .c`.o || exit 1; \
	done
	@test -f $(BUILD)/bearssl/aes_big_cbcdec.o || { echo "BearSSL: no objects built"; exit 1; }
	rm -f $@
	$(AR) $(ARFLAGS) $@ $(BUILD)/bearssl/*.o

$(JSONC_LIB):
	@echo "Building json-c..."
	@test -d $(BUILD) || mkdir $(BUILD)
	@test -d $(BUILD)/json-c || mkdir $(BUILD)/json-c
	@for f in $(JSONC_SRC); do \
		$(CC) $(OPT) $(WARN) $(DEFS) $(COMPAT_INC) -I$(TP)/json-c -c $(TP)/json-c/$$f \
			-o $(BUILD)/json-c/`basename $$f .c`.o || exit 1; \
	done
	rm -f $@
	$(AR) $(ARFLAGS) $@ $(BUILD)/json-c/*.o

# gcc build on Linux, for testing. Compiles into build-linux/ (no incremental build).
linux:
	rm -f sginet-linux
	$(MAKE) PLATFORM=linux CC=gcc OPT="-O2 -g" WARN="-Wall -Wno-unused-variable -Wno-unused-but-set-variable" \
		BR_PLATFORM= sginet-linux

sginet-linux: $(BEARSSL_LIB) $(JSONC_LIB) $(NCURSES_STAMP)
	@test -d $(BUILD) || mkdir $(BUILD)
	@test -d $(BUILD)/app || mkdir $(BUILD)/app
	@for f in $(SRC); do \
		$(CC) $(CFLAGS) -c $$f -o $(BUILD)/app/`basename $$f .c`.o || exit 1; \
	done
	$(CC) $(OPT) $(BUILD)/app/*.o -o sginet-linux $(NCURSES_LIBS) $(JSONC_LIB) $(BEARSSL_LIB) $(SYSLIBS)

xsginet-linux: FORCE
	$(MAKE) PLATFORM=linux CC=gcc OPT="-O2 -g" WARN="-Wall -Wno-unused-variable -Wno-unused-but-set-variable" \
		BR_PLATFORM= X11_TARGET=xsginet-linux X11_LDFLAGS= xsginet

# Mesa GLwDrawA.h declares the widget class without extern: -fcommon, and -u pulls
# the Motif widget out of libGLw.a (the tentative definition in main.o would hide it)
xsginet-globe-linux: FORCE
	$(MAKE) PLATFORM=linux CC=gcc OPT="-O2 -g -fcommon" WARN="-Wall -Wno-unused-variable -Wno-unused-but-set-variable" \
		BR_PLATFORM= GLOBE_TARGET=xsginet-globe-linux X11_LDFLAGS= GLOBE_LDFLAGS=-Wl,-u,GLwCreateMDrawingArea xsginet-globe

# always rebuild the Linux X11 binaries
FORCE:

# gcc on Linux: syntax check as C89 like IRIX 5 cc sees it (IRIX-only code is not checked)
C89FLAGS = -std=c89 -pedantic-errors -Wno-long-long -Wno-overlength-strings \
	-Werror=declaration-after-statement -Werror=vla -fsyntax-only \
	-D_DEFAULT_SOURCE -DIRIX5 -Dinline= -DBR_64=0 -DBR_INT128=0 -DBR_UMUL128=0 -isystem compat/irix5
c89check:
	@for f in $(SRC) compat/irix5/snprintf.c; do \
		gcc $(C89FLAGS) -Ibuild-linux/ncurses/include $(INCLUDES) $$f || exit 1; \
	done
	@for f in $(X11_SRC); do \
		gcc $(C89FLAGS) $(INCLUDES) $$f || exit 1; \
	done
	@for f in $(GLOBE_SRC); do \
		gcc $(C89FLAGS) $(INCLUDES) $(GLOBE_INC) $$f || exit 1; \
	done
	@for f in $(JSONC_SRC); do gcc $(C89FLAGS) -I$(TP)/json-c $(TP)/json-c/$$f || exit 1; done
	@for f in `find $(TP)/bearssl/src -name '*.c' -print`; do \
		gcc $(C89FLAGS) -I$(TP)/bearssl/inc -I$(TP)/bearssl/src $$f || exit 1; \
	done
	@echo "c89check OK"

clean:
	rm -f *.o compat/irix5/*.o sginet sginet-linux xsginet xsginet-linux xsginet-globe xsginet-globe-linux
	rm -rf build-linux/app build-linux/x11 build-irix/x11 build-linux/globe build-irix/globe

# removes BearSSL and json-c builds (ncurses is kept, it takes long to build on old machines)
clean-deps:
	rm -rf $(BUILD)/bearssl $(BUILD)/json-c $(BEARSSL_LIB) $(JSONC_LIB)
