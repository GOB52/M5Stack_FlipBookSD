#!/bin/bash
#
# Convert all GCF to GMV in current directory
#
for fname in ./*.gcf
do
    python ./gcf_to_gmv.py ${fname%.*}.gcf ${fname%.*}.wav ${fname%.*.*}.gmv
    echo ${fname%.*.*}.gmv done
done
exit 0

