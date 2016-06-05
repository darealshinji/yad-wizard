#!/bin/sh

## The MIT License (MIT)
##
## Copyright (c) 2016, djcj <djcj@gmx.de>
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to deal
## in the Software without restriction, including without limitation the rights
## to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
## copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included in all
## copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.

scriptdir="$(cd "$(dirname "$0")" && pwd)"
cd "$scriptdir"

# # check if the Xserver is running on this display (what about Wayland)?
# if ! xset q &>/dev/null; then
# 	echo "error: no X server running at [`cat /sys/class/tty/tty0/active`]" >&2
# 	exit 1
# fi


# ##### Settings ##### #

# # the application's name
# appname="My App"

# # lowercase name without spaces and special chars
# app_lower="my_app"

# # start-up command inside the app directory
# cmd="start-app.sh"

# # icon for .desktop files, PNGs are preferred
# app_icon="app-icon.png"

# # categories for the desktop menu
# # https://standards.freedesktop.org/menu-spec/latest/apa.html
# #
# # valid main categories are:
# #  AudioVideo
# #  Audio
# #  Video
# #  Development
# #  Education
# #  Game
# #  Graphics
# #  Network
# #  Office
# #  Science
# #  Settings
# #  System
# #  Utility
# categories="Game;"

# # tarball with the files to install;
# # supported filetypes/extensions:
# # .tar.gz/.tgz, .tar.bz2/.tbz, .tar.xz/.txz
# tarball="test.txz"

# # use embedded xz decompressor
# xzminidec="yes"

# source settings from external file
. ./settings

# #################### #


width=480
height=300
width_l=540
height_l=400

# sometimes the windows aren't centered; this is a workaround
if xrandr --version &>/dev/null ; then
	screenwidth=$(xrandr -q | grep -w Screen | sed 's|.*current ||; s|,.*||; s| x .*||')
	screenheight=$(xrandr -q | grep -w Screen | sed 's|.*current ||; s|,.*||; s|.* x ||')
	pointX=$(( ($screenwidth - $width) / 2 ))
	pointY=$(( ($screenheight - $height) / 2 ))
	geometry="--geometry=+$pointX+$pointY"
	pointX_l=$(( ($screenwidth - $width_l) / 2 ))
	pointY_l=$(( ($screenheight - $height_l) / 2 ))
	geometry_l="--geometry=+$pointX_l+$pointY_l"
fi

# x86 only for now
if [ "x$arch" = "x" ]; then
	test "$(uname -m)" = "x86_64" && arch="x86_64" || arch="x86"
fi

window_icon="setup.png"

yad_bin="yad.$arch"
yad_common="--window-icon=./$window_icon --center --on-top --fixed"
yad_opts="$yad_common --width=$width --height=$height $geometry"
yad_l_opts="$yad_common --width=$width_l --height=$height_l $geometry_l"
yad="./$yad_bin $yad_opts"
yad_l="./$yad_bin $yad_l_opts"

pv="$PWD/pv.$arch"
unxz="unxz"
test "$xzminidec" = "TRUE" -o "$xzminidec" = "yes" && unxz="$PWD/xzminidec.$arch"
tarball="$scriptdir/$tarball"


# ##### Welcome page / Settings ##### #
selection="""$($yad --title="Welcome" \
		--text="Welcome to this assistent." \
		--text-align=center \
		--form --separator='|' \
		--field="Install files to::CDIR" "$HOME/$app_lower" \
		--field="Create a desktop menu entry:CHK" \
		--field="Add a launcher to the user desktop:CHK" \
		--button="Install" \
		--button="Cancel")"""
test $? -ne 0 && exit 1
path="$(echo $selection | cut -d '|' -f1)"
test "$(echo $selection | cut -d '|' -f2)" = "TRUE" && with_desktop_menu=yes
test "$(echo $selection | cut -d '|' -f3)" = "TRUE" && with_launcher=yes


# ##### Path checks ##### #
if [ -d "$path" ]; then
	if [ -w "$path" ]; then
		if [ "x$(ls -A "$path")" != "x" ]; then
			if ! $yad --title="$title" \
					--button="Continue" \
					--button="Cancel" \
					--text="\nThe selected directory is not empty.\nExisting files will be replaced if you continue!" \
					--text-align=center
			then
				exit 1
			fi
		fi
	else
		$yad --title="$title" \
			--button="Close" \
			--text="\nCan't write files into the selected directory." \
			--text-align=center
		exit 1
	fi
else
	if ! mkdir -p "$path" 2>/dev/null; then
		$yad --title="$title" \
			--button="Close" \
			--text="\nCan't create the specified directory." \
			--text-align=center
		exit 1
	fi
fi


# ##### License agreement ##### #
bunzip2 LICENSE.bz2
if ! $yad_l --title="License" \
		--text-info \
		--wrap \
		--filename="./LICENSE" \
		--button="Accept" \
		--button="Decline"
then
	exit 1
fi


# ##### Installation ##### #
appdir="$path/app"
localapps="$HOME/.local/share/applications"
desktop_entry="${app_lower}-$(mktemp -u XXXXXXXX).desktop"
log=$(mktemp -u)

if xdg-user-dir &>/dev/null ; then
	desktop="$(xdg-user-dir DESKTOP 2>/dev/null)"
else
	with_launcher=no
fi

case "$(printf "$tarball" | tail -c3)" in
	txz|.xz)
		decompress="$unxz"
	;;
	tbz|bz2)
		decompress="bunzip"
	;;
	tgz|.gz)
		decompress="gunzip"
	;;
		*)
		$yad --title="$title" \
			--button="Close" \
			--text="Unsupported file format: $tarball" \
			--text-align=center
		exit 1
	;;
esac

# Create safe and unique filenames to avoid mistakes
# when we're going to invoke pidof and pkill.
pv_tmp=$(mktemp -u)
yad_tmp=$(mktemp -u)
ln -s "$pv" $pv_tmp
ln -s "$PWD/$yad_bin" $yad_tmp

mkdir -p "$appdir" "$path/.uninstall"
cd "$appdir"
($pv_tmp -n "$tarball" | $decompress | tar xf -) 2>&1 | \
	tee $log | \
	$yad_tmp $yad_opts \
		--progress \
		--progress-text="Installing files" \
		--percentage=2 \
		--auto-close \
		--button="Cancel" &
cd "$scriptdir"

# Start a loop to constantly check if yad is still running and kill pv
# if it isn't. Nothing will happen if the extraction was already
# completed, but the process will be cancelled if it wasn't.
running=0
while [ $running -eq 0 ]; do
	if [ -z "$(pidof $yad_tmp)" ]; then
		pkill -f $pv_tmp
		running=1
	fi
done

# Installation cancelled
if [ "x$(tail -n1 $log)" != "x100" ]; then
	"$yad" $yad_opts \
		--progress \
		--progress-text="Cancelled" \
		--percentage=0 \
		--button="Close"
	rm -f $log $pv_tmp $yad_tmp
	exit 0
fi

# if the app directory contains nothing but a single subdirectory,
# move the subdirectory's content into the app directory
if [ "x$(ls -1A "$appdir" | wc -l)" = "x1" ] && \
   [ -d "$appdir/$(ls -1A "$appdir")" ];
then
	tmpdir="$appdir/$(mktemp -u XXXXXX)"
	mv "$appdir/$(ls -1A "$appdir")" "$tmpdir" && \
	mv "$tmpdir"/* "$appdir" && \
	rmdir "$tmpdir"
fi

# .desktop file
cat <<EOF> "$path/$desktop_entry"
[Desktop Entry]
Name=$appname
Exec=$appdir/$cmd
Type=Application
Categories=$categories
StartupNotify=true
Icon=$path/.icon
EOF
chmod 0755 "$path/$desktop_entry"

# copy icons
cp "$scriptdir/$app_icon" "$path/.icon"
cp "$scriptdir/$window_icon" "$path/.uninstall"

# user desktop menu entry
if [ "x$with_desktop_menu" = "xyes" ]; then
	mkdir -p "$localapps"
	cp "$path/$desktop_entry" "$localapps/$desktop_entry"
fi

# launcher on user desktop
if [ "x$with_launcher" = "xyes" ]; then
	test -w "$desktop" && install -m 0755 "$path/$desktop_entry" "$desktop/$desktop_entry"
fi

# generate uninstall script
cat <<EOF> "$path/uninstall.sh"
#!/bin/sh
if "$path/.uninstall/yad.$arch" --center --on-top \
	--window-icon="$path/.uninstall/icon.png" \
	--text-align=left \
	--text="\nDo you really want to remove this application and all its components?\n" \
	--button="Uninstall" \
	--button="Cancel"
then
	test -f "$localapps/$desktop_entry" && rm -v "$localapps/$desktop_entry"
	test -f "$desktop/$desktop_entry" && rm -v "$desktop/$desktop_entry"
	test -d "$path" && rm -rv "$path"
fi
exit 0
EOF
chmod 0755 "$path/uninstall.sh"

cp -f ./$yad_bin "$path/.uninstall"
rm -f $log $pv_tmp $yad_tmp


# ##### Finished ##### #
$yad $yad_opts \
	--progress \
	--progress-text="Complete" \
	--percentage=100 \
	--button="Done"
exit 0

