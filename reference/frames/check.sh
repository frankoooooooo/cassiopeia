#!/bin/zsh
# Runs operator-design's checks on one terrace engine's exported frames; results land in reference/frames/<id>/results.txt.
# reference/frames/check.sh schedar 5,4,5,3   (audible-effect scores per control, in order)
E=$1; SCORES=${2:-5,4,5,3}
OD=~/Documents/make/Code/operator-design
R=${0:A:h:h:h}
F=$R/reference/frames/$E
NAMES=$(python3 -c "import json;print(','.join(p['id'] for p in json.load(open('$R/$E/definition.json'))['parameters']))")
cd $OD
{
  echo "== lint_definition"; python3 opxy/tools/lint_definition.py $R/$E/definition.json
  echo "== lint_frame (frames)"; python3 opxy/tools/lint_frame.py $F/*.svg --definition $R/$E/definition.json
  { echo '<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220">'; cat $R/$E/panel.svg.fragment; echo '</svg>'; } > $F/fragment.svg.tmp
  echo "== lint_frame (artwork)"; python3 opxy/tools/lint_frame.py $F/fragment.svg.tmp --fragment --definition $R/$E/definition.json
  echo "== perceptibility"; python3 opxy/tools/perceptibility.py $F --scores $SCORES --names $NAMES
  echo "== contact sheet"; python3 opxy/tools/contact_sheet.py $F --out $F/contact.svg --png
} > $F/results.txt 2>&1
rm -f $F/fragment.svg.tmp
tail -5 $F/results.txt; echo "full results: $F/results.txt"
