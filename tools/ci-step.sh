#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# One step of the Build workflow (.github/workflows/build.yml), shown as a progress bar: the
# whole build's, once a minute while tools/build.sh's steps run, and a line on the run's
# summary page for each step done. The steps' output stays in .build/logs (kept with the
# run); the end of it is shown only when a step fails.
#
#   tools/ci-step.sh NUMBER [BUILD_STEP...]
#
# NUMBER is the workflow step's (see NAMES); without build steps only the bar is printed.
set -uo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
names=("" "Packages" "Sources" "Title (PC5 itself)" "RADV (the PS5's Vulkan driver)" "Wine for this computer"
       "Wine for the PS5" "32-bit games' CPU" "DXVK" "Zink (OpenGL)" "Package, app and sources" "The archives"
       "Draft release")
minutes=(0 1 2 1 6 60 45 10 15 60 15 1 3)   # about, on GitHub's machines
total=$(( ${#names[@]} - 1 ))
n=$1
shift
all=0
for (( i = 1; i <= total; i++ )); do all=$(( all + minutes[i] )); done
before=0
for (( i = 1; i < n; i++ )); do before=$(( before + minutes[i] )); done

# The bar: the whole build, as far as the estimates go, with this step `spent` minutes in.
bar() { # spent
    local spent=$1 part width=40 done_minutes left filled
    part=$(( spent < minutes[n] ? spent : minutes[n] ))
    done_minutes=$(( before + part ))
    left=$(( all - done_minutes ))
    (( left < 1 )) && left=1
    filled=$(( done_minutes * width / all ))
    printf '%s%s  %3d%%  step %d of %d: %s  (%d min in, about %d h %02d min left)\n' \
        "$(printf '%*s' "$filled" '' | sed 's/ /█/g')" "$(printf '%*s' $(( width - filled )) '' | sed 's/ /░/g')" \
        $(( done_minutes * 100 / all )) "$n" "$total" "${names[n]}" "$spent" $(( left / 60 )) $(( left % 60 ))
}

bar 0
[[ $# -gt 0 ]] || exit 0
started=$SECONDS status=0
for step in "$@"; do
    mkdir -p "$here/.build/logs"
    bash "$here/tools/build.sh" "$step" > /dev/null 2>&1 &
    builder=$!
    while kill -0 "$builder" 2>/dev/null; do
        for (( s = 0; s < 60; s++ )); do
            kill -0 "$builder" 2>/dev/null || break
            sleep 1
        done
        kill -0 "$builder" 2>/dev/null && bar $(( (SECONDS - started) / 60 ))
    done
    wait "$builder"
    status=$?
    if (( status )); then
        echo "✗ ${names[n]}: tools/build.sh $step failed. The end of its log:"
        echo "::group::.build/logs/$step.log"
        tail -60 "$here/.build/logs/$step.log" 2>/dev/null
        echo "::endgroup::"
        echo "::error title=${names[n]} failed::tools/build.sh $step failed; the end of its log is in this step"
        break
    fi
done
took=$(( (SECONDS - started + 59) / 60 ))
if (( status == 0 )); then
    echo "✓ ${names[n]} done in $took min"
    if [[ -n ${GITHUB_STEP_SUMMARY:-} ]]; then
        [[ $n == 2 ]] && printf '| Step | | Took |\n| --- | --- | --- |\n' >> "$GITHUB_STEP_SUMMARY"
        printf '| %d of %d | %s | %d min |\n' "$n" "$total" "${names[n]}" "$took" >> "$GITHUB_STEP_SUMMARY"
    fi
fi
exit "$status"
