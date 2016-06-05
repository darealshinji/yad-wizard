#!/bin/sh -e

if [ "$(uname -m)" = "x86_64" ]; then
  target="x86_64"
else
  target="x86"
fi

rev=987
dir="yad-trunk"
rm -rf $dir
svn checkout -r $rev "svn://svn.code.sf.net/p/yad-dialog/code/trunk" $dir

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

