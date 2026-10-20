#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

dry_run=false
case "${1:-}" in
    '') ;;
    --dry-run) dry_run=true ;;
    -h|--help)
        printf 'Usage: %s [--dry-run]\nBuild, validate and install Emerald Glass plugins.\n--dry-run builds and validates without installing.\n' "$0"
        exit 0
        ;;
    *) printf 'Unknown argument: %s\n' "$1" >&2; exit 1 ;;
esac
if (( $# > 1 )); then
    printf 'Expected at most one argument.\n' >&2
    exit 1
fi

cd -- "$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")"
for tool in cmake ninja python3 glslangValidator qmake6; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required tool not found: %s\n' "$tool" >&2
        exit 1
    fi
done
plugin_dir=$(qmake6 -query QT_INSTALL_PLUGINS)
if [[ "$plugin_dir" != /* ]]; then
    printf 'Cannot determine the Qt 6 plugin directory.\n' >&2
    exit 1
fi

# Keep installation builds separate from interactive/research build trees.
build_root="${EMERALD_BUILD_ROOT:-build/install}"
decoration_build="$build_root/decoration"
effect_build="$build_root/effect"
cmake -S . -B "$decoration_build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_KWIN_DECORATION=ON -DBUILD_TESTING=ON
cmake --build "$decoration_build" --parallel "${JOBS:-8}"
ctest --test-dir "$decoration_build" --output-on-failure
cmake -S vendor/kwin-effects-glass -B "$effect_build" -G Ninja \
    -DGLASS_WAYLAND=ON -DGLASS_X11=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$effect_build" --parallel "${JOBS:-8}"
python3 validate_shaders.py

sources=(
    "$decoration_build/native/org.tankred.emeraldglass.so"
    "$decoration_build/native/kcm_emeraldglassdecoration.so"
    "$effect_build/src/emeraldglass.so"
)
targets=(
    "$plugin_dir/org.kde.kdecoration3/org.tankred.emeraldglass.so"
    "$plugin_dir/org.kde.kdecoration3.kcm/kcm_emeraldglassdecoration.so"
    "$plugin_dir/kwin/effects/plugins/emeraldglass.so"
)
for source in "${sources[@]}"; do
    test -f "$source" || { printf 'Build output missing: %s\n' "$source" >&2; exit 1; }
done

privilege=()
if ! "$dry_run" && (( EUID != 0 )); then
    sudo -v
    privilege=(sudo)
fi
for i in "${!sources[@]}"; do
    printf 'Install: %s -> %s\n' "${sources[i]}" "${targets[i]}"
    if ! "$dry_run"; then
        # Replace the inode so libraries already mapped by KWin stay intact.
        pending="${targets[i]}.new.$$"
        "${privilege[@]}" install -Dm755 -- "${sources[i]}" "$pending"
        "${privilege[@]}" mv -f -- "$pending" "${targets[i]}"
    fi
done
if "$dry_run"; then
    printf 'Dry run complete; no plugins installed.\n'
else
    printf 'Installed. Log out and back in to load the updated plugins.\nDecoration selection and effect settings have not been changed.\n'
fi
