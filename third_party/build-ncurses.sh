#!/bin/sh
#
# Build a static ncurses (with form, menu, panel; tinfo merged in) for sginet.
# Result goes to build-$PLATFORM/ncurses/{include,lib} (PLATFORM defaults to irix).
#
# fetch the tarball and save in ./third_party/
#   https://invisible-island.net/archives/ncurses/ncurses-6.5.tar.gz
#
# Usage: sh third_party/build-ncurses.sh  (MIPSpro: CC=c99; IRIX 5.x: cc, o32, ksh)
# For dev tests on linux: PLATFORM=linux sh third_party/build-ncurses.sh
#
# Needs a make that can handle the ncurses Makefiles (GNU make or IRIX smake).
# These are build-time only; the sginet binary does not depend on them.

NCURSES_VER=6.5
NCURSES_SHA256=136d91bc269a9a5785e5f9e980bc76ab57428f604ce3e5a5a90cebc767971cc6
NCURSES_URL=https://invisible-island.net/archives/ncurses/ncurses-$NCURSES_VER.tar.gz

# terminals compiled into the binary, used when the terminfo database has no entry.
# Generating them needs ncurses own tic/infocmp (IRIX SVR4 tools can't), so the
# generated source is kept in third_party/ncurses-$NCURSES_VER-fallback.c.

# To regenerate on Linux: delete that file, run this script, and copy
# ncurses-$NCURSES_VER/ncurses/fallback.c back.
FALLBACKS=iris-ansi,iris-ansi-ap,xterm,vt100,vt220,ansi,rxvt,xterm-256color,screen-256color,tmux-256color

set -e

# portable "command -v" (IRIX /bin/sh may not have it)
have() {
    for d in `echo "$PATH" | tr ':' ' '`; do
        if [ -x "$d/$1" ]; then return 0; fi
    done
    return 1
}

if [ -z "$PLATFORM" ]; then
    PLATFORM=irix
fi

TP=`cd \`dirname $0\` && pwd`
BUILD=`dirname $TP`/build-$PLATFORM
TARBALL=$TP/ncurses-$NCURSES_VER.tar.gz
FALLBACK_SRC=$TP/ncurses-$NCURSES_VER-fallback.c
SRCDIR=$BUILD/ncurses-$NCURSES_VER
PREFIX=$BUILD/ncurses

# IRIX 5.x: no n32 ABI and usually no c99 driver (IDO cc, o32 only)
IRIX5=no
if [ "$PLATFORM" = "irix" ]; then
    case `uname -r` in
    5.*) IRIX5=yes ;;
    esac
fi

if [ -z "$CC" ]; then
    if [ "$PLATFORM" = "irix" ] && [ "$IRIX5" = "no" ]; then CC=c99; else CC=cc; fi
fi
if [ -z "$CFLAGS" ]; then
    # MIPSpro flags only on IRIX; on Linux c99/cc are gcc wrappers
    if [ "$IRIX5" = "no" ] && [ "$PLATFORM" = "irix" ] && { [ "$CC" = "c99" ] || [ "$CC" = "cc" ]; }; then
        CFLAGS="-n32 -mips3 -O2"
    else
        CFLAGS="-O2"
    fi
fi

# IRIX 5.x /bin/sh is a pre-POSIX Bourne shell that can't parse the
# "(pattern)" case syntax in ncurses' configure; use ksh instead.
# configure also puts CONFIG_SHELL into the Makefiles as SHELL.
if [ -z "$CONFIG_SHELL" ] && [ "$IRIX5" = "yes" ]; then
    for s in /bin/ksh /usr/bin/ksh; do
        if [ -x $s ]; then CONFIG_SHELL=$s; break; fi
    done
fi
if [ -z "$CONFIG_SHELL" ]; then
    CONFIG_SHELL=/bin/sh
fi
export CONFIG_SHELL
if [ -z "$MAKE" ]; then
    if have gmake; then MAKE=gmake; else MAKE=make; fi
fi
export CC CFLAGS

if [ ! -f "$TARBALL" ]; then
    echo "Downloading $NCURSES_URL"
    if have curl; then
        curl -fL -o "$TARBALL" "$NCURSES_URL"
    elif have wget; then
        wget -O "$TARBALL" "$NCURSES_URL"
    else
        echo "No downloader found. Copy ncurses-$NCURSES_VER.tar.gz to $TP and re-run." >&2
        exit 1
    fi
fi

if have sha256sum; then
    SUM=`sha256sum "$TARBALL" | cut -d' ' -f1`
elif have shasum; then
    SUM=`shasum -a 256 "$TARBALL" | cut -d' ' -f1`
else
    SUM=""
    echo "WARNING: no sha256 tool, cannot verify $TARBALL (expected $NCURSES_SHA256)"
fi
if [ -n "$SUM" ] && [ "$SUM" != "$NCURSES_SHA256" ]; then
    echo "Checksum mismatch for $TARBALL" >&2
    echo "  expected $NCURSES_SHA256" >&2
    echo "  got      $SUM" >&2
    exit 1
fi

rm -rf "$PREFIX"
# no "mkdir -p" on IRIX 5.x; $BUILD's parent always exists
if [ ! -d "$BUILD" ]; then mkdir "$BUILD"; fi
cd "$BUILD"
# extracting is slow on old machines; delete $SRCDIR to force a fresh tree
if [ ! -f "$SRCDIR/configure" ]; then
    gzip -dc "$TARBALL" | tar xf -
else
    echo "Reusing already extracted $SRCDIR"
fi
cd "$SRCDIR"

# IRIX 5.x head has no -n option; "head -1" works everywhere.
# (no "sed -i" there either)
F=ncurses/base/MKlib_gen.sh
sed 's/head -n 1/head -1/' $F > $F.tmp && cat $F.tmp > $F && rm -f $F.tmp

# IRIX 5.x has no <stdint.h>, so the ABI 6 defaults (uint32_t chtype/mmask_t,
# intptr_t tparm args) don't compile. long is 32 bit in o32, same width.
EXTRA_CONF=
if [ "$IRIX5" = "yes" ]; then
    EXTRA_CONF="--with-chtype=long --with-mmask-t=long --with-tparm-arg=long"
fi

$CONFIG_SHELL ./configure $EXTRA_CONF \
    --prefix="$PREFIX" \
    --without-shared --with-normal --without-debug \
    --without-cxx --without-cxx-binding --without-ada \
    --without-tests --without-manpages \
    --without-termlib --disable-widec \
    --disable-db-install \
    --with-terminfo-dirs=/usr/share/lib/terminfo:/usr/share/terminfo:/usr/lib/terminfo \
    --with-default-terminfo-dir=/usr/share/lib/terminfo \
    --with-fallbacks=$FALLBACKS \
    --enable-overwrite

if [ -f "$FALLBACK_SRC" ]; then
    # newer than MKfallback.sh, so make won't try to regenerate it
    cp "$FALLBACK_SRC" ncurses/fallback.c
    touch ncurses/fallback.c
fi

$MAKE libs
$MAKE install.libs install.includes

echo "ncurses installed to $PREFIX"
