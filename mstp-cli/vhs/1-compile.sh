#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"

#-------------------------------------------------------------------------------
# The initial setup, opening the M-Step and so on
#-------------------------------------------------------------------------------
nix-shell -p vhs --run "vhs ${ROOT}/setup.tape"

#-------------------------------------------------------------------------------
# Separate the trace part to speed up the recording.
# NOTE:
# - To record this trace I have to copy the content of 1-get-trace-gif.sh  to
# substitute the standard 1-run-pocs.sh. This is only for visualization purpose, 
# the original test old take minutes to record.
#-------------------------------------------------------------------------------
# nix-shell -p vhs --run "vhs ${ROOT}/trace.tape"

#-------------------------------------------------------------------------------
# NOTE:
# - To record this trace I have to copy the content of 1-get-recovery-gif.sh  to
# substitute the standard 1-run-pocs.sh. This is only for visualization purpose, 
# the original test old take minutes to record.
#-------------------------------------------------------------------------------
# nix-shell -p vhs --run "vhs ${ROOT}/recovery-key.tape"

#-------------------------------------------------------------------------------
# When all GIFs are generated compile them into a single GIF. 
#-------------------------------------------------------------------------------
# rm -rf mstp-vhs.gif || true

# nix-shell -p ffmpeg --run '\
#     ffmpeg -i mstp-setup.gif \
#            -i mstp-trace.gif \
#            -i mstp-recovery.gif \
#            -filter_complex "[0:v][1:v][2:v]concat=n=3:v=1:a=0[v];[v]split[a][b];[a]palettegen[p];[b][p]paletteuse" \
#            -y mstp-vhs-clean.gif'