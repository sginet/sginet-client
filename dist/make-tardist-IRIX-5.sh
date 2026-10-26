#!/bin/sh
#
# Builds an IRIX 5.x inst tardist package "sginet_client" from the
# natively built IRIX 5 sginet binary (see BUILDING.md, "smake irix5").
#
# The package installs:
#   /usr/bin/sginet                 the sginet binary
#   /usr/bin/xsginet                the X11/Motif client (subsystem x11, if built)
#   /usr/bin/xsginet-globe          the OpenGL globe viewer (subsystem globe, if built)
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
# The package can only be installed on IRIX 5.x: the subsystem has a
# prereq on eoe1.sw.unix, the IRIX 5 base system subsystem (IRIX 6.x
# has eoe.sw.* instead), so inst refuses it on IRIX 6.
#
# Must be run on IRIX 5.x (needs /usr/sbin/gendist from the Software
# Packager, and runs the binary to get its version).
#
# Usage: sh dist/make-tardist-IRIX-5.sh
#
# Environment overrides:
#   SGINET=<file>   sginet binary to package (default ./sginet)
#   XSGINET=<file>  xsginet binary to package (default ./xsginet, skipped if missing)
#   XSGINET_GLOBE=<file>
#                   xsginet-globe binary to package (default ./xsginet-globe, skipped if missing)
#   WORK=<dir>      temporary build directory (default /tmp/sginet-tardist.$$)
#   GENDIST=<cmd>   gendist command (default /usr/sbin/gendist)
#   KEEP=1          keep the temporary build directory
#   FORCE=1         build even when not running on IRIX 5.x
#

# inst product names may only contain letters, digits and underscores -
# a "-" breaks the subsystem expression in the spec (EMPTY product).
PRODUCT=sginet_client
SUBSYS=$PRODUCT.sw.base
SUBSYS_X11=$PRODUCT.sw.x11
SUBSYS_GLOBE=$PRODUCT.sw.globe

# IRIX 5 base system subsystem, required to install the package
PREREQ=eoe1.sw.unix

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

OSREL=`uname -r`
case "$OSREL" in
    5.*) ;;
    *)
        if [ "$FORCE" != "1" ]; then
            echo "This builds the IRIX 5 package and must run on IRIX 5.x (this is $OSREL)." 1>&2
            echo "Use dist/make-tardist-IRIX-6.sh for IRIX 6.5, or set FORCE=1." 1>&2
            exit 1
        fi
        ;;
esac

if [ ! -f "$SGINET" ]; then
    echo "sginet binary $SGINET not found - please run smake irix5 first" 1>&2
    exit 1
fi

if [ ! -f "$DAEMONSCRIPT" ]; then
    echo "Daemon script $DAEMONSCRIPT not found" 1>&2
    exit 1
fi

for f in "$LICENSES" "$INSTREADME" "$INSTREADMEHTML"; do
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
# This also checks the binary actually runs here - an IRIX 6.5 (n32) build
# does not run on IRIX 5, so the build stops here. Not used as the package
# version, only printed below.
SGINET_VERSION=`"$SGINET" -r 2>/dev/null | sed -n 1p | sed 's/[ 	]//g'`
if [ -z "$SGINET_VERSION" ]; then
    echo "Could not get version from $SGINET -r - is it an IRIX 5 build (smake irix5)?" 1>&2
    exit 1
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

TARDIST=$OUT/$PRODUCT-IRIX5-$VERSION.tardist

echo "Building $PRODUCT $VERSION for IRIX 5 (inst version $INSTVERSION)"
echo "Binary:         $SGINET ($SGINET_VERSION)"
if [ -f "$XSGINET" ]; then
    echo "X11 client:     $XSGINET"
else
    echo "X11 client:     $XSGINET not found, building without xsginet (run smake xsginet-irix5)"
    XSGINET=
fi
if [ -f "$XSGINET_GLOBE" ]; then
    echo "Globe viewer:   $XSGINET_GLOBE"
else
    echo "Globe viewer:   $XSGINET_GLOBE not found, building without xsginet-globe (run smake xsginet-globe-irix5)"
    XSGINET_GLOBE=
fi
echo "Work directory: $WORK"

rm -rf "$WORK"
mkdir -p "$STAGE/etc/init.d" "$STAGE/etc/config" "$STAGE/etc/rc2.d" "$STAGE/usr/bin" "$STAGE/$DOCDIR" "$DIST" || exit 1

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

# sort on the 5th field (destination path); old style key syntax for IRIX 5 sort
(
    echo "f 0755 root sys usr/bin/sginet usr/bin/sginet $SUBSYS nostrip norqs"
    if [ -n "$XSGINET" ]; then
        echo "f 0755 root sys usr/bin/xsginet usr/bin/xsginet $SUBSYS_X11 nostrip norqs $REMOVEOP_X11"
    fi
    if [ -n "$XSGINET_GLOBE" ]; then
        echo "f 0755 root sys usr/bin/xsginet-globe usr/bin/xsginet-globe $SUBSYS_GLOBE nostrip norqs $REMOVEOP_GLOBE"
    fi
    echo "f 0755 root sys etc/init.d/sginet-daemon etc/init.d/sginet-daemon $SUBSYS $REMOVEOP"
    echo "f 0644 root sys etc/config/sginet-daemon etc/config/sginet-daemon $SUBSYS config(noupdate)"
    echo "l 0755 root sys etc/rc2.d/S58sginet etc/rc2.d/S58sginet $SUBSYS symval(../init.d/sginet-daemon)"
    echo "d 0755 root sys $DOCDIR $DOCDIR $SUBSYS"
    echo "f 0644 root sys $DOCDIR/LICENSES $DOCDIR/LICENSES $SUBSYS"
) | sort +4 -5 > "$IDB" || exit 1

echo "  `wc -l < "$IDB" | sed 's/ //g'` entries"

# --- spec -----------------------------------------------------------------

echo "Generating $SPEC..."
X11SUBSYS=
if [ -n "$XSGINET" ]; then
    X11SUBSYS="        subsys x11 default
            id \"SGINet X11/Motif client, installed in /usr/bin/xsginet\"
            replaces self
            prereq ( $PREREQ 0 2147483647 )
            exp $SUBSYS_X11
        endsubsys
"
fi
GLOBESUBSYS=
if [ -n "$XSGINET_GLOBE" ]; then
    GLOBESUBSYS="        subsys globe default
            id \"SGINet OpenGL globe viewer, installed in /usr/bin/xsginet-globe\"
            replaces self
            prereq ( $PREREQ 0 2147483647 )
            exp $SUBSYS_GLOBE
        endsubsys
"
fi
cat > "$SPEC" <<EOF
product $PRODUCT
    id "SGINet Client $VERSION (IRIX 5)"
    image sw
        id "SGINet Client Software for IRIX 5"
        version $INSTVERSION
        order 9999
        subsys base default
            id "SGINet Client for IRIX 5, installed in /usr/bin/sginet"
            replaces self
            prereq ( $PREREQ 0 2147483647 )
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
echo "Install as root (IRIX 5.x only) with:"
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
