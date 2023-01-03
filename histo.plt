set style fill solid 0.5 # fill style

set xlabel "Words"
set ylabel "Frequency"
set title "AOAB Weekly Part Sizes"

set terminal png size 800,600

set xtics 1500
set boxwidth 450
set key autotitle columnhead
plot 'histo.dat' using 1:(column(2)+column(3)+column(4)+column(5)+column(6)+column(7)+column(8)+column(9)) with boxes fill pattern 5, '' using 1:(column(2)+column(3)+column(4)+column(5)+column(6)+column(7)+column(8)) with boxes fill pattern 5, '' using 1:(column(2)+column(3)+column(4)+column(5)+column(6)+column(7)) with boxes fill pattern 5, '' using 1:(column(2)+column(3)+column(4)+column(5)+column(6)) with boxes, '' using 1:(column(2)+column(3)+column(4)+column(5)) with boxes, '' using 1:(column(2)+column(3)+column(4)) with boxes, '' using 1:(column(2)+column(3)) with boxes, '' using 1:2 with boxes, 'latest-1.dat' using 1:2:4 with labels offset 1 point pt 7 title column(3), 'latest.dat' using 1:2:4 with labels offset 1 point pt 7 lc rgb"cyan" title column(3)
