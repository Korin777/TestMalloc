#!/usr/bin/env bash

app_all="ll";

dat_dir="out/data";
gp_dir="out/gp";
plot_dir="out/plots"
gp_template="scripts/template.gp";

[ -d "$dat_dir" ] || mkdir -p $dat_dir;
[ -d "$gp_dir" ] || mkdir -p $gp_dir;
[ -d "$plot_dir" ] || mkdir -p $plot_dir;

rm -f $gp_dir/* >> /dev/null;
rm -f $plot_dir/* >> /dev/null;

# initial_all="128 1024 8192";
# update_all="0 10 50";

sizes="8 16 24"
for size in $sizes;
do
# for update in $update_all
# do
#     echo "* Update: $update";
#     for initial in $initial_all
#     do
# 	echo "** Size: $initial";

	for app in $app_all
	do
	    printf "*** Application: $app\n";

	    dat="$dat_dir/$app.s$size.dat";
	    gp="$gp_dir/$app.s$size.gp";
	    png="$plot_dir/$app.s$size.png";
	    title="malloc vs. my\\\_malloc / Malloc\\\_Size: $size";

	    cp $gp_template $gp

	    if [ "$app" = "ll" ];
	    then
		cat << EOF >> $gp
set title "$title"; 
set output "$png";
plot \\
"$dat" using 1:(\$2) title  "lb - Througput" ls 2 with lines, \\
"$dat" using 1:(\$4) axis x1y2 title  "lb - Scalability" ls 1 with points, \\
"$dat" using 1:(\$5) title  "lf - Througput" ls 4 with lines, \\
"$dat" using 1:(\$7) axis x1y2 title  "lf - Scalability" ls 3 with points
EOF
	    fi;

	    gnuplot $gp;
	done;
#     done;
# done;
done;
