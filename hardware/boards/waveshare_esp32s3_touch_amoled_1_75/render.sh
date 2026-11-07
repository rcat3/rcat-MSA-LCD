#!/usr/bin/env bash
#
# Render the speaker housing's parts to STL files for printing:
#
#   ./render.sh                 every part of the housing
#   ./render.sh tests           the test prints, for checking a printer's fit
#   ./render.sh body chin       just these parts
#
# The files go in the stl/ folder next to this script. Parts render in
# parallel, and the body and collar take the longest, around a minute.

cd "$(dirname "$0")"

SCAD=msa_speaker_housing.scad
ALL_PARTS=(body chin back collar washer gasket plungers usb_plug pod_test plunger_test lens_fit)
PART_GROUPS=(housing tests)
GROUP_housing="body chin back collar washer gasket plungers usb_plug"
GROUP_tests="pod_test plunger_test lens_fit"
DEFAULT=(housing)

source ../../common/render_parts.sh
