#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# One step of the Build workflow (.github/workflows/build.yml): a progress bar with the time
# left, then tools/build.sh's steps with their logs shown as they are written, so the run's
# page shows the build as it goes.
#
#   tools/ci-step.sh NUMBER [BUILD_STEP...]
#
# NUMBER is the workflow step's (see NAMES); without build steps only the bar is printed.
set -uo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
names=("" "Packages" "Sources" "Title" "RADV" "Wine for this computer" "Wine for the PS5" "32-bit games' CPU"
       "DXVK" "Zink" "Package, app and sources" "The archives" "Draft release")
minutes=(0 1 2 1 6 60 45 10 15 60 15 1 3)   # about, on GitHub's machines
total=$(( ${#names[@]} - 1 ))
n=$1
shift

left=0
for (( i = n; i <= total; i++ )); do left=$(( left + minutes[i] )); done
all=0
for (( i = 1; i <= total; i++ )); do all=$(( all + minutes[i] )); done
done_part=$(( (all - left) * 30 / all ))
bar=$(printf '%*s' "$done_part" '' | tr ' ' '#')$(printf '%*s' $(( 30 - done_part )) '' | tr ' ' '.')
printf '\n[%s] %d%%  step %d of %d: %s  (about %d min; about %d h %02d min left in all)\n\n' \
    "$bar" $(( (all - left) * 100 / all )) "$n" "$total" "${names[n]}" "${minutes[n]}" $(( left / 60 )) $(( left % 60 ))
echo "::notice title=Step $n of $total::${names[n]}, about ${minutes[n]} min; about $(( left / 60 )) h $(( left % 60 )) min left"

[[ $# -gt 0 ]] || exit 0
status=0
for step in "$@"; do
    log=$here/.build/logs/$step.log
    mkdir -p "$here/.build/logs"
    : > "$log"
    # Wine's builds write make's output to logs of their own.
    deps=$here/.build/prospero-win/.deps
    case $step in
    hostwine) more=("$deps/wine-host/configure.log" "$deps/wine-host/make.log" "$deps/wine-host/install.log") ;;
    wineps5) more=("$deps/wine-ps5/configure.log" "$deps/wine-ps5/make.log") ;;
    *) more=() ;;
    esac
    tail -n +1 -F "$log" "${more[@]}" 2>/dev/null &
    shower=$!
    bash "$here/tools/build.sh" "$step" > /dev/null
    status=$?
    sleep 2
    kill "$shower" 2>/dev/null
    wait "$shower" 2>/dev/null
    if (( status )); then
        echo "::error title=$step failed::see the end of the log above"
        break
    fi
done
exit "$status"
