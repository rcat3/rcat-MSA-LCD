# Renders the parts of an OpenSCAD design to STL files, in parallel. Each
# board folder's render.sh sets these and then sources this file:
#
#   SCAD        the design, relative to the board folder
#   ALL_PARTS   every part it can render, as an array of `part` values
#   OUT         where the STL files go (default: stl)
#
# and passes its own arguments on, so `./render.sh body back` renders just
# those parts and `./render.sh` renders all of them.

set -euo pipefail

OUT=${OUT:-stl}

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
