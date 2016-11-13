#!/bin/sh -e

if [ "$(uname -m)" = "x86_64" ]; then
  target="x86_64"
else
  target="x86"
fi

dir="miniunzip-git"
test -d $dir || git clone https://github.com/nmoinvaz/minizip $dir

libz="libz-1.2.8.2015.12.26"
test -f ${libz}.tar.gz || wget "https://sortix.org/libz/release/${libz}.tar.gz"
rm -rf $libz && tar xf ${libz}.tar.gz

export CFLAGS="-Os -Wformat -Werror=format-security -fno-stack-protector -fno-strict-aliasing -ffunction-sections -fdata-sections -D_FORTIFY_SOURCE=2"
export LDFLAGS="-Wl,-z,defs -Wl,-z,norelro -Wl,--gc-sections -Wl,--as-needed"

cd $libz
./configure --disable-shared
make -j4

cd ../$dir
cat <<EOF> all.c
#include "ioapi.c"
#include "unzip.c"
#include "miniunz.c"
EOF
gcc $CFLAGS -I../$libz -DNOCRYPT -DNOUNCRYPT -DSTDC -D'OF(args)'='args' $LDFLAGS -o ../miniunzip.$target all.c ../$libz/libz.a

cd ..
rm -f sstrip && gcc -s -W -Wall -Wextra -O2 -o sstrip sstrip.c
./sstrip -z miniunzip.$target
upx --best miniunzip.$target

