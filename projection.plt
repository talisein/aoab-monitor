set xlabel "Part"
set ylabel "Words"
set title "AOAB Weekly Parts\nCurrent Volume vs Previous Volume"

set terminal png size 800,600

set key autotitle columnhead

set xtics 1

plot 'latest-1.dat' using 4:5 with linespoints lw 1 lt 3 pt 4 title column(3), 'latest.dat' using 4:5 with linespoints lw 2 lt 4 pt 4 title column(3),  'latest-1-avg-1.dat' with lines lt 0 lc 3 title column(3), 'latest-avg-1.dat' with lines lt 0 lc 4 title column(3), 'latest-proj.dat' with lines lt 0 title column(3);
