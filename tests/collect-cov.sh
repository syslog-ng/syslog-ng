#!/bin/sh
cd ../src

GCDATA="`ls *.gcda 2>/dev/null`"
if [ -z "$GCDATA" ]; then
    echo "syslog-ng was not compiled with coverage information, unable to collect them"
    exit 0
fi
for i in $GCDATA; do
    cfile=`basename $i .gcda`.c;
    gcov $cfile  | grep ^Lines | tail -1 | sed -e "s/^Lines executed:/$cfile,/g" -e 's/ of /,/g' -e 's/%//g';
done | awk -F , '
BEGIN {  };
{
  tested_lines=$3*$2 / 100;
  sum_tested_lines=sum_tested_lines + tested_lines;
  sum_lines=sum_lines+$3; print $1, $2 "%", $3, $3-tested_lines;
};
END {
  print "Total coverage:", sum_tested_lines/sum_lines*100 "%", sum_lines, sum_tested_lines;
};'
