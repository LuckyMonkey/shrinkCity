#!/bin/sh
set -eu

SIM="$1"
DEFS="$($SIM --list-scenarios)"

[ "$(printf '%s\n' "$DEFS" | grep -c '^SCENARIO_DEF ')" -ge 4 ]
printf '%s\n' "$DEFS" | grep -q ' corner-market '
printf '%s\n' "$DEFS" | grep -q ' electronics '
printf '%s\n' "$DEFS" | grep -q ' big-box '
printf '%s\n' "$DEFS" | grep -q ' pharmacy '

STREAM="$($SIM --scenario electronics --seed 2 --stream --ticks 30)"
printf '%s\n' "$STREAM" | grep -q '^SCENARIO 2 electronics '
printf '%s\n' "$STREAM" | grep -q '^GEOMETRY '
printf '%s\n' "$STREAM" | grep -q '^EMPLOYEE '
printf '%s\n' "$STREAM" | grep -q '^EVENT CUSTOMER_ENTERED '

printf '%s\n' 'scenario CLI and authoritative event adapter: ok'
