#!/usr/bin/env bash
#
# Render the housing's parts to STL files for printing.
#
#   ./render.sh                 every part, including the test prints
#   ./render.sh body bezel      just these parts
#
# The files go in the stl/ folder next to this script. They're for the
# RP2040 board; for the RP2350 board, set board = "rp2350" in board.scad first.

cd "$(dirname "$0")"

SCAD=msa_housing.scad
ALL_PARTS=(body bezel gasket phase_test fit_test)

source ../../common/render_parts.sh
