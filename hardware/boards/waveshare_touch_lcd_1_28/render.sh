#!/usr/bin/env bash
#
# Render the housing's parts to STL files for printing:
#
#   ./render.sh                 every part of the housing
#   ./render.sh tests           phase_test, to make the housing sit level, and
#                               fit_test, for checking a printer's fit
#   ./render.sh body bezel      just these parts
#
# The files go in the stl/ folder next to this script. They're for the
# RP2040 board; for the RP2350 board, set board = "rp2350" in board.scad first.

cd "$(dirname "$0")"

SCAD=msa_housing.scad
ALL_PARTS=(body bezel gasket phase_test fit_test)
PART_GROUPS=(housing tests)
GROUP_housing="body bezel gasket"
GROUP_tests="phase_test fit_test"
DEFAULT=(housing)

source ../../common/render_parts.sh
