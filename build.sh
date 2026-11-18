#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
for python in python3 python; do
    if command -v "$python" >/dev/null && "$python" -c 'import sys; sys.exit(sys.version_info < (3, 9))' 2>/dev/null; then
        exec "$python" "$root/scripts/build_library.py" "$@"
    fi
done
printf '%s\n' 'Python 3.9 or newer is required.' >&2
exit 1
