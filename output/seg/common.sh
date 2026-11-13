banner() {
  local title="$1"
  printf '\n\033[1;36m============================================================\n'
  printf '  %s\n' "$title"
  printf '============================================================\033[0m\n'
}
pause() { sleep 1.5; }
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="$ROOT/output"
