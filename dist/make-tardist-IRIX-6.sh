#!/bin/sh
#
# Builds an IRIX 6.5 inst tardist package "sginet_client" from the
# natively built sginet binary (see BUILDING.md, "make").
#
# The package installs:
#   /usr/bin/sginet                 the sginet binary
#   /usr/bin/xsginet                the X11/Motif client (subsystem x11, if built)
#   /usr/bin/xsginet-globe          the OpenGL globe viewer (subsystem globe, if built)
#   /usr/lib/filetype/install/xsginet.ftr, xsginet-globe.ftr
#   /usr/lib/filetype/install/iconlib/xSGINet.fti, xSGINet-globe.fti
#                                   desktop file type rules and icons (from icons/)
#   /usr/lib/desktop/iconcatalog/pages/C/Applications/xsginet, xsginet-globe
#                                   Icon Catalog entries (Applications page)
#   /etc/init.d/sginet-daemon       init script (from daemon/sginet-daemon)
#   /etc/config/sginet-daemon       chkconfig flag, set to "on"
#   /etc/rc2.d/S58sginet            autostart link
#   /usr/share/doc/sginet_client/LICENSES
#                                   SGINet and third-party licenses (from dist/LICENSES)
#
# dist/inst.README and dist/inst.README.html are added to the top level
# of the tardist (next to the product files) as the intro message for
# inst and the Software Manager.
#
# On removal the running sginet daemon is stopped and inst removes all
# of the above.
#
# Must be run on IRIX (needs /usr/sbin/gendist from the Software Packager).
#
# Usage: sh dist/make-tardist-IRIX-6.sh
#
# Environment overrides:
#   SGINET=<file>   sginet binary to package (default ./sginet)
#   XSGINET=<file>  xsginet binary to package (default ./xsginet, skipped if missing)
#   XSGINET_GLOBE=<file>
#                   xsginet-globe binary to package (default ./xsginet-globe, skipped if missing)
#   WORK=<dir>      temporary build directory (default /tmp/sginet-tardist.$$)
#   GENDIST=<cmd>   gendist command (default /usr/sbin/gendist)
#   KEEP=1          keep the temporary build directory
#

# inst product names may only contain letters, digits and underscores -
# a "-" breaks the subsystem expression in the spec (EMPTY product).
PRODUCT=sginet_client
SUBSYS=$PRODUCT.sw.base
SUBSYS_X11=$PRODUCT.sw.x11
SUBSYS_GLOBE=$PRODUCT.sw.globe

dir=`/bin/dirname "$0"`
case "$dir" in
    /*) ;;
    *)  dir="`/bin/pwd`/$dir" ;;
esac
ROOT=`cd "$dir/.." && /bin/pwd`
SGINET=${SGINET:-$ROOT/sginet}
XSGINET=${XSGINET:-$ROOT/xsginet}
XSGINET_GLOBE=${XSGINET_GLOBE:-$ROOT/xsginet-globe}
DAEMONSCRIPT=$ROOT/daemon/sginet-daemon
ICONS=$ROOT/icons
FTRDIR=usr/lib/filetype/install
ICONLIBDIR=$FTRDIR/iconlib
ICONCATDIR=usr/lib/desktop/iconcatalog/pages/C/Applications
OUT=$ROOT/dist
LICENSES=$OUT/LICENSES
INSTREADME=$OUT/inst.README
INSTREADMEHTML=$OUT/inst.README.html
DOCDIR=usr/share/doc/$PRODUCT

WORK=${WORK:-/tmp/sginet-tardist.$$}
GENDIST=${GENDIST:-/usr/sbin/gendist}
STAGE=$WORK/stage
IDB=$WORK/$PRODUCT.idb
SPEC=$WORK/$PRODUCT.spec
DIST=$WORK/dist

if [ ! -f "$SGINET" ]; then
    echo "sginet binary $SGINET not found - please run make first" 1>&2
    exit 1
fi

if [ ! -f "$DAEMONSCRIPT" ]; then
    echo "Daemon script $DAEMONSCRIPT not found" 1>&2
    exit 1
fi

for f in "$LICENSES" "$INSTREADME" "$INSTREADMEHTML" \
         "$ICONS/xsginet.ftr" "$ICONS/xSGINet.fti" \
         "$ICONS/xsginet-globe.ftr" "$ICONS/xSGINet-globe.fti"; do
    if [ ! -f "$f" ]; then
        echo "$f not found" 1>&2
        exit 1
    fi
done

if [ "$GENDIST" = "/usr/sbin/gendist" ] && [ ! -x /usr/sbin/gendist ]; then
    echo "/usr/sbin/gendist not found - please install the IRIX Software Packager (swpkg)" 1>&2
    exit 1
fi

# Client version as reported by the binary being packaged, e.g. V20261015-1311-CL2.
# Informational only, printed below.
SGINET_VERSION=`"$SGINET" -r 2>/dev/null | head -1 | sed 's/[ 	]//g'`
if [ -z "$SGINET_VERSION" ]; then
    echo "Could not get version from $SGINET -r" 1>&2
    SGINET_VERSION=unknown
fi

# Package version from the build time, not from the sginet binary: a new
# package can contain the same sginet but an updated xsginet or readme.
# One date call, so the tardist name and the inst version match.
STAMP=`date +%Y%m%d%H%M`
DATE=`echo "$STAMP" | cut -c1-8`
TIME=`echo "$STAMP" | cut -c9-12`
VERSION=V$DATE-$TIME

# inst needs an integer version: use YYYYMMDDHH (fits into 32 bit)
INSTVERSION=`echo "$STAMP" | cut -c1-10`

TARDIST=$OUT/$PRODUCT-$VERSION.tardist

echo "Building $PRODUCT $VERSION (inst version $INSTVERSION)"
echo "Binary:         $SGINET ($SGINET_VERSION)"
if [ -f "$XSGINET" ]; then
    echo "X11 client:     $XSGINET"
else
    echo "X11 client:     $XSGINET not found, building without xsginet (run make xsginet)"
    XSGINET=
fi
if [ -f "$XSGINET_GLOBE" ]; then
    echo "Globe viewer:   $XSGINET_GLOBE"
else
    echo "Globe viewer:   $XSGINET_GLOBE not found, building without xsginet-globe (run make xsginet-globe)"
    XSGINET_GLOBE=
fi
echo "Work directory: $WORK"

rm -rf "$WORK"
mkdir -p "$STAGE/etc/init.d" "$STAGE/etc/config" "$STAGE/etc/rc2.d" "$STAGE/usr/bin" "$STAGE/$DOCDIR" \
    "$STAGE/$ICONLIBDIR" "$STAGE/$ICONCATDIR" "$DIST" || exit 1

# --- staging tree ---------------------------------------------------------

echo "Copying sginet binary to staging area..."
cp "$SGINET" "$STAGE/usr/bin/sginet" || exit 1
chmod 755 "$STAGE/usr/bin/sginet"
if [ -n "$XSGINET" ]; then
    cp "$XSGINET" "$STAGE/usr/bin/xsginet" || exit 1
    chmod 755 "$STAGE/usr/bin/xsginet"
fi
if [ -n "$XSGINET_GLOBE" ]; then
    cp "$XSGINET_GLOBE" "$STAGE/usr/bin/xsginet-globe" || exit 1
    chmod 755 "$STAGE/usr/bin/xsginet-globe"
fi

echo "Copying desktop file type rules, icons and Icon Catalog links..."
if [ -n "$XSGINET" ]; then
    cp "$ICONS/xsginet.ftr" "$STAGE/$FTRDIR/xsginet.ftr" || exit 1
    cp "$ICONS/xSGINet.fti" "$STAGE/$ICONLIBDIR/xSGINet.fti" || exit 1
    chmod 644 "$STAGE/$FTRDIR/xsginet.ftr" "$STAGE/$ICONLIBDIR/xSGINet.fti"
    ln -s /usr/bin/xsginet "$STAGE/$ICONCATDIR/xsginet" || exit 1
fi
if [ -n "$XSGINET_GLOBE" ]; then
    cp "$ICONS/xsginet-globe.ftr" "$STAGE/$FTRDIR/xsginet-globe.ftr" || exit 1
    cp "$ICONS/xSGINet-globe.fti" "$STAGE/$ICONLIBDIR/xSGINet-globe.fti" || exit 1
    chmod 644 "$STAGE/$FTRDIR/xsginet-globe.ftr" "$STAGE/$ICONLIBDIR/xSGINet-globe.fti"
    ln -s /usr/bin/xsginet-globe "$STAGE/$ICONCATDIR/xsginet-globe" || exit 1
fi

echo "Creating init script, chkconfig flag and autostart link..."
cp "$DAEMONSCRIPT" "$STAGE/etc/init.d/sginet-daemon" || exit 1
chmod 755 "$STAGE/etc/init.d/sginet-daemon"

echo "on" > "$STAGE/etc/config/sginet-daemon"
chmod 644 "$STAGE/etc/config/sginet-daemon"

ln -s ../init.d/sginet-daemon "$STAGE/etc/rc2.d/S58sginet" || exit 1

echo "Copying license file..."
cp "$LICENSES" "$STAGE/$DOCDIR/LICENSES" || exit 1
chmod 644 "$STAGE/$DOCDIR/LICENSES"

# --- idb ------------------------------------------------------------------

echo "Generating $IDB..."

# stop the daemon before inst removes the files
REMOVEOP='removeop("/etc/killall -TERM sginet 2>/dev/null; true")'
REMOVEOP_X11='removeop("/etc/killall -TERM xsginet 2>/dev/null; true")'
REMOVEOP_GLOBE='removeop("/etc/killall -TERM xsginet-globe 2>/dev/null; true")'
# recompile the desktop file type database so the new rules and icons show up
FTRMAKE='exitop("if test -r $rbase/usr/lib/filetype/Makefile; then chroot $rbase /sbin/sh -c \"cd /usr/lib/filetype; make -u > /dev/null 2>&1\"; fi; true")'

(
    echo "f 0755 root sys usr/bin/sginet usr/bin/sginet $SUBSYS nostrip"
    if [ -n "$XSGINET" ]; then
        echo "f 0755 root sys usr/bin/xsginet usr/bin/xsginet $SUBSYS_X11 nostrip $REMOVEOP_X11"
        echo "f 0644 root sys $FTRDIR/xsginet.ftr $FTRDIR/xsginet.ftr $SUBSYS_X11 $FTRMAKE"
        echo "f 0644 root sys $ICONLIBDIR/xSGINet.fti $ICONLIBDIR/xSGINet.fti $SUBSYS_X11"
        echo "l 0755 root sys $ICONCATDIR/xsginet $ICONCATDIR/xsginet $SUBSYS_X11 symval(/usr/bin/xsginet)"
    fi
    if [ -n "$XSGINET_GLOBE" ]; then
        echo "f 0755 root sys usr/bin/xsginet-globe usr/bin/xsginet-globe $SUBSYS_GLOBE nostrip $REMOVEOP_GLOBE"
        echo "f 0644 root sys $FTRDIR/xsginet-globe.ftr $FTRDIR/xsginet-globe.ftr $SUBSYS_GLOBE $FTRMAKE"
        echo "f 0644 root sys $ICONLIBDIR/xSGINet-globe.fti $ICONLIBDIR/xSGINet-globe.fti $SUBSYS_GLOBE"
        echo "l 0755 root sys $ICONCATDIR/xsginet-globe $ICONCATDIR/xsginet-globe $SUBSYS_GLOBE symval(/usr/bin/xsginet-globe)"
    fi
    echo "f 0755 root sys etc/init.d/sginet-daemon etc/init.d/sginet-daemon $SUBSYS $REMOVEOP"
    echo "f 0644 root sys etc/config/sginet-daemon etc/config/sginet-daemon $SUBSYS config(noupdate)"
    echo "l 0755 root sys etc/rc2.d/S58sginet etc/rc2.d/S58sginet $SUBSYS symval(../init.d/sginet-daemon)"
    echo "d 0755 root sys $DOCDIR $DOCDIR $SUBSYS"
    echo "f 0644 root sys $DOCDIR/LICENSES $DOCDIR/LICENSES $SUBSYS"
) | sort -k 5,5 > "$IDB" || exit 1

echo "  `wc -l < "$IDB" | sed 's/ //g'` entries"

# --- spec -----------------------------------------------------------------

echo "Generating $SPEC..."
X11SUBSYS=
if [ -n "$XSGINET" ]; then
    X11SUBSYS="        subsys x11 default
            id \"SGINet X11/Motif client, installed in /usr/bin/xsginet\"
            replaces self
            exp $SUBSYS_X11
        endsubsys
"
fi
GLOBESUBSYS=
if [ -n "$XSGINET_GLOBE" ]; then
    GLOBESUBSYS="        subsys globe default
            id \"SGINet OpenGL globe viewer, installed in /usr/bin/xsginet-globe\"
            replaces self
            exp $SUBSYS_GLOBE
        endsubsys
"
fi
cat > "$SPEC" <<EOF
product $PRODUCT
    id "SGINet Client $VERSION"
    image sw
        id "SGINet Client Software"
        version $INSTVERSION
        order 9999
        subsys base default
            id "SGINet Client, installed in /usr/bin/sginet"
            replaces self
            exp $SUBSYS
        endsubsys
$X11SUBSYS$GLOBESUBSYS    endimage
endproduct
EOF

# --- gendist --------------------------------------------------------------

echo "Running gendist..."
$GENDIST -rbase / -sbase "$STAGE" -idb "$IDB" -spec "$SPEC" -dist "$DIST" -all
if [ $? -ne 0 ]; then
    echo "gendist failed, work directory kept: $WORK" 1>&2
    exit 1
fi

# intro message shown by inst / swmgr when the distribution is opened
cp "$INSTREADME" "$DIST/inst.README" || exit 1
cp "$INSTREADMEHTML" "$DIST/inst.README.html" || exit 1
chmod 644 "$DIST/inst.README" "$DIST/inst.README.html"

# --- tardist --------------------------------------------------------------

if [ ! -f "$DIST/$PRODUCT" ]; then
    echo "gendist produced no $DIST/$PRODUCT, work directory kept: $WORK" 1>&2
    exit 1
fi

echo "Creating $TARDIST..."
rm -f "$TARDIST"
# the product description file must come first
(cd "$DIST" && tar cvf "$TARDIST" $PRODUCT `ls | grep -v "^$PRODUCT\$"`) || exit 1

if [ "$KEEP" = "1" ]; then
    echo "Work directory kept: $WORK"
else
    rm -rf "$WORK"
fi

echo ""
echo "Done: $TARDIST"
echo ""
echo "Install as root with:"
echo "  inst -f $TARDIST"
echo "or open it in the Software Manager (swmgr)."
echo ""
echo "Note: the (optional) sginet-daemon is enabled but not started. Register your SGI by"
echo "running sginet (/usr/bin/sginet) first, then start the daemon with:"
echo "  /etc/init.d/sginet-daemon start"
echo ""
echo "Remove with:"
echo "  versions remove $PRODUCT"
echo ""
