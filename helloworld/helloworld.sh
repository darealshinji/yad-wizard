#!/bin/sh
[ $(uname -m) = x86_64 ] && arch=x86_64 || arch=x86
cd "$(dirname "$0")" && ./helloworld.$arch $*
