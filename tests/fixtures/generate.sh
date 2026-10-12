#! /bin/sh
# Regenerates the detector fixtures with Audio Video Sync Dock's generator at the commit the detector derives from.
# Usage: generate.sh [Sync Dock checkout]
# Without an argument it clones Sync Dock to a temporary directory. Pillow and qrcode must be importable, for example
# from "pip install -r tool/requirements.txt" in the checkout. The fixtures are committed, so the tests never run this.
set -e

commit=e4ad2941ed5f2982e9f17b76ebaf22096319f3cb
here=$(cd "$(dirname "$0")" && pwd)

if [ -n "$1" ]; then
	checkout=$1
else
	checkout=$(mktemp -d)/obs-audio-video-sync-dock
	git clone --quiet https://github.com/norihiro/obs-audio-video-sync-dock.git "$checkout"
	git -C "$checkout" checkout --quiet "$commit"
fi

found=$(git -C "$checkout" rev-parse HEAD)
if [ "$found" != "$commit" ]; then
	echo "Sync Dock is at $found, expected $commit" >&2
	exit 1
fi

python3 "$here/generate.py" "$checkout" "$here"
