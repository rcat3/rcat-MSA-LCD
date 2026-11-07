#!/usr/bin/env bash
#
# Render the housings' parts to STL files for printing:
#
#   ./render.sh                 both housings
#   ./render.sh simple          the simple housing (msa_housing.scad)
#   ./render.sh speaker         the speaker housing (msa_speaker_housing.scad)
#   ./render.sh tests           the test prints, for checking a printer's fit
#   ./render.sh body back       just these parts
#
# The files go in the stl/ folder next to this script. Parts render in
# parallel, and the speaker housing's body and collar take the longest,
# around a minute.

cd "$(dirname "$0")"

SCAD=msa_speaker_housing.scad
ALL_PARTS=(msa_housing.scad:housing body back collar washer gasket plungers spacers
           msa_housing.scad:fit_test msa_housing.scad:lens_fit msa_housing.scad:thread_test pod_test plunger_test)
PART_GROUPS=(simple speaker tests)
GROUP_simple="housing"
GROUP_speaker="body back collar washer gasket plungers spacers"
GROUP_tests="fit_test lens_fit thread_test pod_test plunger_test"
DEFAULT=(simple speaker)

source ../../common/render_parts.sh
