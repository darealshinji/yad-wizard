#!/bin/sh -e

if [ "$(uname -m)" = "x86_64" ]; then
  target="x86_64"
else
  target="x86"
fi

dir="xz-embedded"
test -d $dir || {
  git clone "http://git.tukaani.org/xz-embedded.git"
  tag="$(git -C $dir describe --tags)"
  rm -rf $dir/.git
  tar cfJ xzminidec-${tag}.tar.xz $dir
}

CFLAGS="-Os -Wformat -Werror=format-security -fno-stack-protector -fno-strict-aliasing -ffunction-sections -fdata-sections -D_FORTIFY_SOURCE=2"
LDFLAGS="-Wl,-z,defs -Wl,-z,norelro -Wl,--gc-sections -Wl,--as-needed"

make -C $dir/userspace clean
rm -f $dir/userspace/*.o

make -j4 -C $dir/userspace CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"
cp -f $dir/userspace/xzminidec xzminidec.$target

rm -f sstrip && gcc -s -W -Wall -Wextra -O2 -o sstrip sstrip.c
./sstrip -z xzminidec.$target
upx --best xzminidec.$target

