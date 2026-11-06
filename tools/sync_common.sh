#!/bin/sh
# schedar/cz_common.h is the source; copies it into every other engine folder. --check only compares.
cd "$(dirname "$0")/.."
status=0
for m in caph navi ruchbah segin achird; do
  [ -d $m ] || continue
  if [ "$1" = --check ]; then cmp -s schedar/cz_common.h $m/cz_common.h || { echo "$m/cz_common.h differs from schedar's"; status=1; }
  else cp schedar/cz_common.h $m/cz_common.h; fi
done
exit $status
