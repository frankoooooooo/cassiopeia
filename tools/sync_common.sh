#!/bin/sh
# cz101/cz_common.h is the source; copies it into every other engine folder. --check only compares.
cd "$(dirname "$0")/.."
status=0
for m in czanalog czvector czscan czvosim czwalsh; do
  [ -d $m ] || continue
  if [ "$1" = --check ]; then cmp -s cz101/cz_common.h $m/cz_common.h || { echo "$m/cz_common.h differs from cz101's"; status=1; }
  else cp cz101/cz_common.h $m/cz_common.h; fi
done
exit $status
