#!/bin/sh -e

if [ "$(uname -m)" = "x86_64" ]; then
  target="x86_64"
else
  target="x86"
fi

ver="0.36.3"
dir="yad-$ver"
txz="${dir}.tar.xz"
rm -rf $dir
test -f $txz || wget "https://downloads.sourceforge.net/project/yad-dialog/$txz"
tar xf $txz

cd $dir
patch -p1 < ../yad-small-patch.diff
autoreconf -if
XCFLAGS="-Os -Wformat -Werror=format-security -fno-stack-protector -fno-strict-aliasing -ffunction-sections -fdata-sections -D_FORTIFY_SOURCE=2"
XLDFLAGS="-Wl,-z,defs -Wl,-z,norelro -Wl,--gc-sections -Wl,--as-needed"
./configure --prefix=/usr --with-gtk=gtk2 --disable-gio

make clean
make -j4 V=1 XCFLAGS="$XCFLAGS" XLDFLAGS="$XLDFLAGS"
cp -f src/yad ../yad.$target

cd ..
rm -f sstrip && gcc -s -W -Wall -Wextra -O2 -o sstrip sstrip.c
./sstrip -z yad.$target
upx --best yad.$target

