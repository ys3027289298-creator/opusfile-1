#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
banner "Segment 3/4: Acceptance checks (round-trip / empty / invalid / gain)"
pause
echo '$ ./output/run_tests.sh   (acceptance section)'
pause
"$HERE/run_tests.sh" 2>&1 | sed -n '/Running acceptance/,$p'
pause
