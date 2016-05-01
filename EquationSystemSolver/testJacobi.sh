#! /bin/sh -
i=0; while [ "$i" -lt 1000 ]; do
  ./TCgenPos eq ans
  ./TCcalcJacobi eq | ./TCcheckPos ans || break
  i=$((i + 1))
done
