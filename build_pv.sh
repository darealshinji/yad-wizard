#!/bin/sh -e

if [ "$(uname -m)" = "x86_64" ]; then
  target="x86_64"
else
  target="x86"
fi

version="1.6.0"

dir="pv-${version}"
tarball="pv-${version}.tar.bz2"
test -f $tarball || wget -O $tarball "http://www.ivarch.com/programs/sources/$tarball"
test -d $dir || tar xf $tarball

cd $dir
export CFLAGS="-Os -Wformat -Werror=format-security -fno-stack-protector -fno-strict-aliasing -ffunction-sections -fdata-sections -D_FORTIFY_SOURCE=2"
export LDFLAGS="-Wl,-z,defs -Wl,-z,norelro -Wl,--gc-sections -Wl,--as-needed"
./configure --prefix=/usr --disable-nls --disable-splice --disable-ipc --disable-debugging

make clean
make -j4 V=1
cp -f pv ../pv.$target

cd ..
rm -f sstrip && gcc -s -W -Wall -Wextra -O2 -o sstrip sstrip.c
./sstrip -z pv.$target
upx --best pv.$target
mv pv.$target bin

