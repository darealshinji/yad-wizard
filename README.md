This is a simple [YAD](https://sourceforge.net/projects/yad-dialog/)-based setup wizard for Linux or similar Unix systems.
This is supposed to be an easier to hack alternative to [MojoSetup](http://www.icculus.org/mojosetup/) or Qt.
Use `gen_wizard.sh` to create an installer. Alternatively you can manually write a `settings` file according
to the documentation found in the comments of `wizard.sh` and then use `bin/makeself`.

Limitations:
 * right now only xz/gz/bz2 compressed tar archives are supported (can't extract zip and 7z through pipes)
 * no non-graphical fallback alternative (but you can still run `--noexec --target <dir>`)

