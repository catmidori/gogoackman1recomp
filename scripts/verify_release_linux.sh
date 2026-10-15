#!/bin/sh
# Verify an exported build without installing the application into the user profile.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
stage=${ACKMAN_RELEASE_STAGE:-${XDG_CACHE_HOME:-$HOME/.cache}/ackmanrecomp-release}
files=$stage/.flatpak-build/files
probe=$(mktemp -d "$stage/release-probe.XXXXXX")
mkdir -p "$probe/include/desktop"
cp "$root/external/snesrecomp/runner/src/desktop/sdl_compat.h" "$probe/include/desktop/"
archive=$stage/.flatpak-builder/downloads/12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785/SDL3-3.4.10.tar.gz
mkdir "$probe/SDL3"
tar -xf "$archive" -C "$probe/SDL3" --strip-components=1 SDL3-3.4.10/include
flatpak run --filesystem="$stage" --filesystem="$root":ro --command=sh org.freedesktop.Sdk//26.08 -c '
    set -eu
    cc "$1/tests/test_release_paths.c" "$1/src/recomp_menu_paths.c" \
      -I"$1/src" -I"$2/include" -I"$2/SDL3/include" \
      -DSNESRECOMP_SDL3=1 -DRECOMP_MENU_FLATPAK=1 \
      -DRECOMP_MENU_FLATPAK_APP_ID=\"org.snesrecomp.AckmanRecomp\" \
      -DRECOMP_MENU_INSTALLED_ASSETS=\"$3/share/org.snesrecomp.AckmanRecomp\" \
      -L"$3/lib" -lSDL3 -Wl,-rpath,"$3/lib" -o "$2/probe"
    XDG_CONFIG_HOME="$2/xdg" "$2/probe"
' sh "$root" "$probe" "$files"
python3 "$root/scripts/verify_release_layout.py" --linux-prefix "$files"
repo=$probe/import-repo
ostree --repo="$repo" init --mode=archive-z2
flatpak build-import-bundle "$repo" "$root/releases/AckmanRecomp-x86_64.flatpak"
printf 'PASS Flatpak bundle imported into an isolated OSTree repository.\n'
