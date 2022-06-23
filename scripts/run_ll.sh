#!/usr/bin/env bash

out_dir="out/data";
cores="all";
duration="2000";

[ -d "$out_dir" ] || mkdir -p $out_dir;

# settings

# initials="128 1024 8192";
# updates="0 10 50";

sizes="8 16 24"


for size in $sizes;
do
# for initial in $initials; 
# do
#     echo "* -i$initial";

#     range=$((2*$initial));

#     for update in $updates;
#     do
# 	echo "** -u$update";

        out="$out_dir/ll.s$size.dat";
        scripts/scalability2.sh "$cores" \
            main main_MY_MALLOC \
            -d$duration -s$size -c1 | tee $out;
#     done
# done
done
