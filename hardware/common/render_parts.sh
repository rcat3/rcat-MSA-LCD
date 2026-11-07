# Renders the parts of OpenSCAD designs to STL files, in parallel. Each board
# folder's render.sh sets these and then sources this file:
#
#   SCAD        the design, relative to the board folder
#   ALL_PARTS   every part it can render, as an array of `part` values. A part
#               from another design in the folder is written `design.scad:part`.
#   PART_GROUPS names for sets of parts, as an array, each with a variable
#               GROUP_<name> listing its parts, like GROUP_tests="fit_test".
#   DEFAULT     what's rendered when no parts are given: parts or groups
#               (default: every part)
#   OUT         where the STL files go (default: stl)
#
# and passes its own arguments on, so `./render.sh body back` renders just
# those parts, `./render.sh tests` renders a group, and `./render.sh` renders
# the default ones. It sticks to what the bash that comes with macOS has.

set -euo pipefail

OUT=${OUT:-stl}

if ! command -v openscad >/dev/null; then
    echo "OpenSCAD isn't installed. Get it from https://openscad.org/" >&2
    exit 1
fi

usage() {
    echo "Parts: $(for p in "${ALL_PARTS[@]}"; do printf '%s ' "${p#*:}"; done)" >&2
    for g in ${PART_GROUPS[@]+"${PART_GROUPS[@]}"}; do
        var="GROUP_$g"
        echo "  $g: ${!var}" >&2
    done
}

requested=("$@")
if [ ${#requested[@]} -eq 0 ]; then
    if [ -n "${DEFAULT+x}" ]; then
        requested=("${DEFAULT[@]}")
    else
        requested=("${ALL_PARTS[@]#*:}")
    fi
fi

# Expand the groups, then find each part's design
jobs_list=()
for name in "${requested[@]}"; do
    names="$name"
    for g in ${PART_GROUPS[@]+"${PART_GROUPS[@]}"}; do
        if [ "$name" = "$g" ]; then
            var="GROUP_$g"
            names="${!var}"
        fi
    done
    for part in $names; do
        entry=""
        for p in "${ALL_PARTS[@]}"; do
            if [ "${p#*:}" = "$part" ]; then
                entry="$p"
            fi
        done
        if [ -z "$entry" ]; then
            echo "Unknown part or group '${part}'." >&2
            usage
            exit 1
        fi
        if [ "${entry%%:*}" = "$entry" ]; then
            entry="$SCAD:$entry"
        fi
        jobs_list+=("$entry")
    done
done

mkdir -p "$OUT"

render() {
    local scad=${1%%:*}
    local part=${1#*:}
    local log
    if log=$(openscad -D "part=\"${part}\"" -o "${OUT}/${part}.stl" "$scad" 2>&1); then
        echo "Rendered ${OUT}/${part}.stl"
    else
        echo "Failed to render ${part}:" >&2
        echo "$log" >&2
        return 1
    fi
}
export -f render
export OUT

jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
printf '%s\n' "${jobs_list[@]}" | xargs -P "$jobs" -I{} bash -c 'render "$@"' _ {}
