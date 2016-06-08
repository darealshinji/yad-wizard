#!/bin/sh

# The MIT License (MIT)
#
# Copyright (c) 2016, djcj <djcj@gmx.de>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

scriptdir="$(cd "$(dirname "$0")" && pwd)"
cd "$scriptdir"

width=540
height=400
screenwidth=$(xrandr -q | grep -w Screen | sed 's|.*current ||; s|,.*||; s| x .*||')
screenheight=$(xrandr -q | grep -w Screen | sed 's|.*current ||; s|,.*||; s|.* x ||')
pointX=$(( ($screenwidth - $width) / 2 ))
pointY=$(( ($screenheight - $height) / 2 ))

test "$(uname -m)" = "x86_64" && arch="x86_64" || arch="x86"
yad="bin/yad.$arch"
window_icon="./setup.png"

error () {
	$yad --window-icon=$window_icon \
		--center \
		--width=$width \
		--height=$height \
		--geometry=+$pointX+$pointY \
		--title="Error" \
		--image="error" \
		--text-align=center \
		--text="Error:\n$1" \
		--button="Close"
	exit 1
}

arch_both="x86 and x86_64"

selection="""$($yad \
	--window-icon=$window_icon \
	--center \
	--width=$width \
	--height=$height \
	--geometry=+$pointX+$pointY \
	--title="Generate a simple YAD-based installer" \
	--form \
	--align=right \
	--separator='|' \
	--item-separator=',' \
	--field="Application name" "" \
	--field="Lowercase name;\nno spaces or special chars" "" \
	--field="tarball with the files to install:FL" "NULL" \
	--field="Start-up command inside the directory" "" \
	--field="License text (optional):FL" "NULL" \
	--field="Icon:FL" "NULL" \
	--field="Category:CB" "Game,AudioVideo,Audio,Video,Development,Education,Graphics,Network,Office,Science,Settings,System,Utility" \
	--field="Architecture:CB" "$arch_both,x86,x86_64" \
	--field="Use embedded xz decompressor (xzminidec):CHK" \
	--file-filter="all (*)|*" \
	--file-filter="tar archives (*.tar.gz  *.tgz  *.tar.bz2  *.tbz  *.tar.xz  *.txz)|*.tar.gz *.tgz *.tar.bz2 *.tbz *.tar.xz *.tbx" \
	--file-filter="image files (*.png  *.jpg  *.xpm)|*.png *.jpg *.xpm" \
	--file-filter="text files (*.txt)|*.txt" \
	--button="Generate installer" \
	--button="Quit")"""
test $? -ne 0 && exit 1

name="$(echo $selection | cut -d '|' -f1)"
short="$(echo $selection | cut -d '|' -f2)"
tarball="$(echo $selection | cut -d '|' -f3)"
command="$(echo $selection | cut -d '|' -f4)"
license="$(echo $selection | cut -d '|' -f5)"
icon="$(echo $selection | cut -d '|' -f6)"
category="$(echo $selection | cut -d '|' -f7)"
arch="$(echo $selection | cut -d '|' -f8)"
xzminidec="$(echo $selection | cut -d '|' -f9)"

test -n "$name" || error "empty name"
test -n "$short" || error "empty short name"

if [ -z "$command" ]; then
	error "empty command"
else
	command="$(echo "$command" | sed 's|^\.\/||')"
fi

if [ "$(basename "$tarball")" = "NULL" ]; then
	error "no tarball selected"
else
	test -f "$tarball" || error "selected tarball is not a file"
fi

if [ "$(basename "$license")" = "NULL" ]; then
	license=""
else
	test -f "$license" || error "selected license is not a file"
fi

if [ "$(basename "$icon")" = "NULL" ]; then
	error "no icon selected"
else
	test -f "$icon" || error "selected icon is not a file"
fi

cat <<EOL
name: $name
short: $short
tarball: $tarball
command: $command
license: $license
icon: $icon
category: $category
xzminidec: $xzminidec
EOL

tmpdir="$(mktemp -d)"

cp -l "$tarball" $tmpdir
if [ -f "$license" ]; then
	cp "$license" $tmpdir/LICENSE
	bzip2 $tmpdir/LICENSE
fi
cp "$icon" $tmpdir/_icon
cp setup.png $tmpdir

if [ "x$arch" = "x$arch_both" ]; then
	cp bin/pv.x86 bin/pv.x86_64 bin/yad.x86 bin/yad.x86_64 $tmpdir
	test $xzminidec = FALSE || cp bin/xzminidec.x86 bin/xzminidec.x86_64 $tmpdir
else
	cp bin/pv.$arch bin/yad.$arch $tmpdir
	test $xzminidec = FALSE || cp bin/xzminidec.$arch $tmpdir
fi

sed -e '/^# /d; s|^##|#|g; s|^[ \t]*||; /^$/d' wizard.sh | \
	sed -e :a -e '/\\$/N; s|\\\n||; ta' > $tmpdir/wizard.sh
chmod 0775 $tmpdir/wizard.sh

cat <<EOF> $tmpdir/settings
appname="$name"
app_lower="$short"
cmd="$command"
app_icon="_icon"
categories="$category;"
tarball="$(basename "$tarball")"
xzminidec="$xzminidec"
EOF
test "x$arch" != "x$arch_both" && echo "arch=\"$arch\"" >> $tmpdir/settings

if bin/makeself.sh --header bin/makeself-header --nocomp --nox11 $tmpdir ${short}_setup.sh "$name" ./wizard.sh; then
	$yad --window-icon=$window_icon \
			--center \
			--width=$width \
			--height=$height \
			--geometry=+$pointX+$pointY \
			--title="Success" \
			--image="ok" \
			--text-align=center \
			--text="\`${short}_setup.sh' successfully generated." \
			--button="Close"
fi

