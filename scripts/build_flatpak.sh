#!/bin/sh
# Native Linux build; dependencies are downloaded before the sandboxed build.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
command -v flatpak >/dev/null
command -v flatpak-builder >/dev/null
command -v python3 >/dev/null
# Some builder versions have no --clear-cache option. In that case the scoped
# cleaner below purges the project's cache metadata instead of failing early.
if flatpak-builder --help | grep -q -- '--clear-cache'; then
    flatpak-builder --clear-cache
fi
python3 scripts/clean_flatpak_build.py
# The manifest prepares line endings and patches after source extraction.
# Do not mutate the host source tree or CMake's inputs during compilation.
arch=${ARCH:-$(flatpak --default-arch)}
manifest=org.snesrecomp.AckmanRecomp.yaml
mkdir -p dist
# Parse with Flatpak's own loader before downloading/building anything.
flatpak-builder --show-manifest "$manifest" > dist/flatpak-manifest.json
flatpak-builder --force-clean --disable-cache --assumeyes --user --arch="$arch" --install-deps-from=flathub \
    --repo=.flatpak-repo .flatpak-build "$manifest"
flatpak build-bundle --arch="$arch" \
    --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo \
    .flatpak-repo "dist/AckmanRecomp-$arch.flatpak" org.snesrecomp.AckmanRecomp
printf 'Bundle: %s/dist/AckmanRecomp-%s.flatpak\n' "$root" "$arch"
mkdir -p releases
cp "dist/AckmanRecomp-$arch.flatpak" "releases/AckmanRecomp-$arch.flatpak"
python3 scripts/write_release_checksums.py
printf 'Release: %s/releases/AckmanRecomp-%s.flatpak\n' "$root" "$arch"
