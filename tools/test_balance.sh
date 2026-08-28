#!/bin/sh
set -eu

BALANCE="$1"
OUTPUT="$($BALANCE --days 1 --seeds 2 --start-seed 8 --scenario all)"

printf '%s\n' "$OUTPUT" | head -n 1 | grep -q '^seed,layout,scenario,changes,'
[ "$(printf '%s\n' "$OUTPUT" | wc -l)" -eq 13 ]
for scenario in authored no_cameras extra_cameras no_guard extra_guard lean_staff; do
    printf '%s\n' "$OUTPUT" | grep -q ",$scenario,"
done

# All scenarios for a seed must preserve the authoritative layout id.
LAYOUTS="$(printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 {print $2}' | sort -u | wc -l)"
[ "$LAYOUTS" -eq 1 ]

# Scenario application should actually change the intended management lever.
printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 && $3 == "no_cameras" {if ($4 < 1) exit 1}'
printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 && $3 == "extra_cameras" {if ($4 != 2) exit 1}'
printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 && $3 == "no_guard" {if ($4 < 1 || $8 >= 4) exit 1}'
printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 && $3 == "extra_guard" {if ($4 != 1 || $8 <= 4) exit 1}'
printf '%s\n' "$OUTPUT" | awk -F, '$1 == 8 && $3 == "lean_staff" {if ($4 != 1 || $8 >= 4) exit 1}'
