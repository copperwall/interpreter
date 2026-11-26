#!/usr/bin/env bash
# Runs every example and diffs its output against expected/<name>.out.
#
#   ./run.sh                 # use ../build/dev/clox
#   CLOX=../build/release/clox ./run.sh
#   ./run.sh --update        # regenerate the expected outputs
#   ./run.sh 05_bst.lox      # run only the named examples
set -u
cd "$(dirname "$0")"

CLOX="${CLOX:-../build/dev/clox}"
update=0
if [ "${1:-}" = "--update" ]; then update=1; shift; fi

if [ ! -x "$CLOX" ]; then
  echo "clox binary not found at $CLOX (run 'make' in the parent directory)" >&2
  exit 1
fi

files=("$@")
[ ${#files[@]} -eq 0 ] && files=([0-9]*.lox)

pass=0; fail=0
for f in "${files[@]}"; do
  name="${f%.lox}"
  expected="expected/$name.out"
  actual="$("$CLOX" "$f" 2>&1)"; status=$?
  if [ $update -eq 1 ]; then
    printf '%s\n' "$actual" > "$expected"
    echo "wrote $expected"
    continue
  fi
  if [ $status -eq 0 ] && diff -u "$expected" <(printf '%s\n' "$actual") > /dev/null; then
    echo "PASS $f"; pass=$((pass + 1))
  else
    echo "FAIL $f (exit $status)"
    diff -u "$expected" <(printf '%s\n' "$actual") | head -20
    fail=$((fail + 1))
  fi
done

[ $update -eq 1 ] && exit 0
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
