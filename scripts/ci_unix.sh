#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
export VCPKG_ROOT="$PWD/.ci-vcpkg"
jobs="${JOBS:-3}"
mode=$(printf '%s' "$CONFIGURATION" | tr '[:upper:]' '[:lower:]')
cli=(--cli)
if [[ ${LIBRARY_ONLY:-false} == true ]]; then
  cli=()
fi
python_bindings=()
if [[ ${PYTHON_BINDINGS:-false} == true ]]; then
  python_bindings=(--python --python-executable "$(command -v python)")
fi
dotnet_bindings=()
if [[ ${DOTNET_BINDINGS:-false} == true ]]; then
  dotnet_bindings=(--dotnet)
fi
bash build.sh --frontend "$FRONTEND" "--$mode" "--$LIBRARY" --triplet "$VCPKG_TRIPLET" \
  "${cli[@]}" "${python_bindings[@]}" "${dotnet_bindings[@]}" --tests --jobs "$jobs" --build-dir build-ci/objects --install-prefix build-ci/package
if [[ ${PYTHON_BINDINGS:-false} == true ]]; then
  platlib=$(python bindings/python/python_config.py platlib)
  PYTHONPATH="$PWD/build-ci/package/$platlib" python tests/integration/python_bindings.py
fi
if [[ ${LIBRARY_ONLY:-false} != true ]]; then
  build-ci/package/bin/MediaCap --version
  build-ci/package/bin/MediaCap capabilities formats --json
fi
