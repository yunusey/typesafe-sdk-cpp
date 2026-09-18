#!/usr/bin/env bash
# Format (or check) C++ sources with clang-format, using .clang-format at the
# repo root. clang-format is provided by the nix dev shell (llvm.clang-tools).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

usage() {
  cat <<'EOF'
Usage: scripts/format.sh [--check] [path ...]

Run clang-format on C++ sources (.hpp, .h, .hh, .cpp, .cc, .cxx).

  --check   Do not write files; exit 1 if any file would change
  path ...  Format only these files or directories
            (default: include src examples tests testing)

Environment:
  CLANG_FORMAT   clang-format binary (default: clang-format)
EOF
}

CHECK=0
PATHS=()
while (($#)); do
  case "$1" in
    -h | --help)
      usage
      exit 0
      ;;
    --check)
      CHECK=1
      shift
      ;;
    --)
      shift
      PATHS+=("$@")
      break
      ;;
    -*)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      PATHS+=("$1")
      shift
      ;;
  esac
done

if ((${#PATHS[@]} == 0)); then
  PATHS=(include src examples tests testing)
fi

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
  echo "error: $CLANG_FORMAT not found (enter the nix dev shell, or set CLANG_FORMAT)" >&2
  exit 127
fi

if [[ ! -f "$ROOT/.clang-format" ]]; then
  echo "error: missing $ROOT/.clang-format" >&2
  exit 1
fi

files=()
while IFS= read -r -d '' file; do
  files+=("$file")
done < <(
  find "${PATHS[@]}" \
    \( -name '*.hpp' -o -name '*.h' -o -name '*.hh' -o -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) \
    -type f -print0 | sort -z
)

if ((${#files[@]} == 0)); then
  echo "no C++ sources under: ${PATHS[*]}" >&2
  exit 1
fi

if ((CHECK)); then
  "$CLANG_FORMAT" --dry-run --Werror -style=file "${files[@]}"
else
  "$CLANG_FORMAT" -i -style=file "${files[@]}"
fi
