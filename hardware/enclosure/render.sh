#!/usr/bin/env bash
#
# Render the speaker housing's parts to STL files for printing.
#
#   ./render.sh                 every part, including the test prints
#   ./render.sh body back       just these parts
#
# The files go in the stl/ folder next to this script. Parts render in
# parallel, and the body and collar take the longest, around a minute.

set -euo pipefail

cd "$(dirname "$0")"

SCAD=msa_1_46_speaker_housing.scad
OUT=stl
ALL_PARTS=(body back collar washer gasket plungers spacers pod_test plunger_test)

if ! command -v openscad >/dev/null; then
    echo "OpenSCAD isn't installed. Get it from https://openscad.org/" >&2
    exit 1
fi

parts=("$@")
if [ ${#parts[@]} -eq 0 ]; then
    parts=("${ALL_PARTS[@]}")
fi
for part in "${parts[@]}"; do
    if [[ ! " ${ALL_PARTS[*]} " =~ " ${part} " ]]; then
        echo "Unknown part '${part}'. Choose from: ${ALL_PARTS[*]}" >&2
        exit 1
    fi
done

mkdir -p "$OUT"

render() {
    local part=$1
    local log
    if log=$(openscad -D "part=\"${part}\"" -o "${OUT}/${part}.stl" "$SCAD" 2>&1); then
        echo "Rendered ${OUT}/${part}.stl"
    else
        echo "Failed to render ${part}:" >&2
        echo "$log" >&2
        return 1
    fi
}
export -f render
export SCAD OUT

jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
printf '%s\n' "${parts[@]}" | xargs -P "$jobs" -I{} bash -c 'render "$@"' _ {}
