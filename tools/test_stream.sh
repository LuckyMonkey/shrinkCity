#!/bin/sh
set -eu
SIM="$1"
output=$("$SIM" --stream --ticks 3 --seed 12345)
lines=$(printf '%s\n' "$output" | awk 'BEGIN { ticks=0; entities=0 } /^TICK / { ticks++ } /^ENTITY / { entities++ } END { print ticks, entities }')
set -- $lines
[ "$1" -eq 3 ]
[ "$2" -ge 1 ]
printf '%s\n' "shrink-stream: 3 frames and $2 entities validated"
