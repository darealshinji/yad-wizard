#!/bin/sh -e

if [ "$(uname -m)" = "x86_64" ]; then
  target="x86_64"
else
  target="x86"
fi

dir="tiny7zx"
test -d $dir || git clone https://github.com/pts/pts-tiny-7z-sfx $dir

CFLAGS="-Os -Wformat -Werror=format-security -fno-stack-protector -fno-strict-aliasing -ffunction-sections -fdata-sections -D_FORTIFY_SOURCE=2"
LDFLAGS="-Wl,-z,defs -Wl,-z,norelro -Wl,--gc-sections -Wl,--as-needed"

gcc -Wall -I $dir $CFLAGS $LDFLAGS -o tiny7zx.$target $dir/all.c

rm -f sstrip && gcc -s -W -Wall -Wextra -O2 -o sstrip sstrip.c
./sstrip -z tiny7zx.$target
upx --best tiny7zx.$target

