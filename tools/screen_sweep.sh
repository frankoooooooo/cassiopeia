#!/bin/sh
# Runs every engine's real display() under the sanitizers (overflow, bad memory) at each control's
# extremes, at rest and just after a note; any report fails. CI runs it; so can you:
#   sh tools/screen_sweep.sh [path to open-xy/sdk/include] [c++ compiler] [engines…, default all six]
set -e
cd "$(dirname "$0")/.."
SDK=${1:-../open-xy-staging/sdk/include}; CXX=${2:-c++}
[ $# -gt 2 ] && shift 2 && ENGINES="$*" || ENGINES="schedar caph navi ruchbah segin achird"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
for e in $ENGINES; do
  $CXX -std=c++17 -O1 -fsanitize=undefined,address -fno-sanitize-recover=all -Wall -Wextra -Werror -DENGINE=$e -I "$SDK" reference/frames/dump.cpp -o "$T/d"
  for a in 0 0.5 1; do for b in 0 1; do for c in 0 0.5 0.999; do for d in 0 1; do for m in rest playing; do
    "$T/d" "$T/frame.txt" $m $a $b $c $d
  done; done; done; done; done
  echo "$e: clean"
done
