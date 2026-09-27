# Sourced by both GBA launchers. picoarch keeps each core's saves apart
# ($HOME/.picoarch/data/<core>/<rom>.sav); copy the newer in-game save to the
# other core so progress carries over whichever launcher you use. Save
# states (.st*) are core-specific and stay put.
sync_sav() {
	name="${1##*/}"
	name="${name%.*}"
	a="$HOME/.picoarch/data/gpsp/$name.sav"
	b="$HOME/.picoarch/data/mgba/$name.sav"
	mkdir -p "$HOME/.picoarch/data/gpsp" "$HOME/.picoarch/data/mgba"
	if [ -f "$a" ] && { [ ! -f "$b" ] || [ "$a" -nt "$b" ]; }; then
		cp -p "$a" "$b"
	elif [ -f "$b" ] && { [ ! -f "$a" ] || [ "$b" -nt "$a" ]; }; then
		cp -p "$b" "$a"
	fi
	sync
}
