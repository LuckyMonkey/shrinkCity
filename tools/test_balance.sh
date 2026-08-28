#!/bin/sh
set -eu

BALANCE="$1"
OUTPUT="$($BALANCE --days 1 --seeds 2 --start-seed 8 --scenario all)"

printf '%s\n' "$OUTPUT" | head -n 1 | grep -q '^seed,layout,scenario,security_changes,'
[ "$(printf '%s\n' "$OUTPUT" | wc -l)" -eq 7 ]
printf '%s\n' "$OUTPUT" | grep -q ',authored,'
printf '%s\n' "$OUTPUT" | grep -q ',no_cameras,'
printf '%s\n' "$OUTPUT" | grep -q ',extra_cameras,'

# All three scenarios for a seed must preserve the authoritative layout id.
LAYOUTS="$(printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 {print $2}' | sort -u | wc -l)"
[ "$LAYOUTS" -eq 1 ]

# Scenario application should actually change security fixtures for the altered cases.
printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 && $3 == "no_cameras" {if ($4 < 1) exit 1}'
printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 && $3 == "extra_cameras" {if ($4 != 2) exit 1}'
