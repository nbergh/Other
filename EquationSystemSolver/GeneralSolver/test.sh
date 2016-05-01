#! /bin/sh -
i=0; while [ "$i" -lt 1000 ]; do
  ./TCgen eq
  ./TCcalc eq | ./TCcheck eq || break
  i=$((i + 1))
done
