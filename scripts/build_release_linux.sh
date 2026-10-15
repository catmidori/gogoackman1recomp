#!/bin/sh
# Build on a Linux filesystem, including when sources live on a WSL mount.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
stage=${ACKMAN_RELEASE_STAGE:-${XDG_CACHE_HOME:-$HOME/.cache}/ackmanrecomp-release}
case "$stage" in
    /*) ;;
    *) printf 'Release staging must be an absolute Linux path.\n' >&2; exit 1 ;;
esac
if [ "$stage" = "$root" ] || [ -L "$stage" ]; then
    printf 'Release staging must be separate from the source tree and not a symlink.\n' >&2
    exit 1
fi
mkdir -p "$stage"
# Explicit source allowlist: no ROMs, local configuration, saves, mods or bundles.
tar -C "$root" --exclude=.git --exclude=__pycache__ -cf - \
    CMakeLists.txt framework.lock rom_identity.txt icon.png README.md LICENSE \
    THIRD_PARTY.md THIRD_PARTY_ATTRIBUTION.md RELEASE_NOTES.md \
    org.snesrecomp.AckmanRecomp.desktop org.snesrecomp.AckmanRecomp.yaml \
    src recomp assets cmake scripts tests patches docs external/imgui external/nanosvg \
    rom/Instructions.txt mods/Instructions.txt saves/Instructions.txt shaders/Instructions.txt |
    tar -C "$stage" -xf -
(cd "$stage" && sh scripts/build_flatpak.sh)
arch=${ARCH:-$(flatpak --default-arch)}
mkdir -p "$root/releases" "$root/dist"
cp "$stage/dist/AckmanRecomp-$arch.flatpak" "$root/dist/"
cp "$stage/releases/AckmanRecomp-$arch.flatpak" "$root/releases/"
python3 "$root/scripts/write_release_checksums.py"
printf 'Release: %s/releases/AckmanRecomp-%s.flatpak\n' "$root" "$arch"
