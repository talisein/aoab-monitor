n=20 #number of intervals
max=20000. #max value
min=7000. #min value
width=(max-min)/n #interval width
#function used to map a value to the intervals
hist(x,width)=width*floor(x/width)+width/2.0
set boxwidth width*0.9
set style fill solid 0.5 # fill style

set xlabel "Words"
set ylabel "Frequency"
set title "AOAB Weekly Part Sizes"

set jitter vertical spread 0.4

set terminal png size 800,600
#count and plot
plot 'hist.dat' using (hist($1, width)):(1.0) smooth freq w boxes lc rgb"green" title "Cumulative Weekly Parts", 'latest.dat' using 1:2:4 with labels offset 1 point pt 7 title column(3), 'latest-1.dat' using 1:2 with points pt 7 title column(3), 'latest-2.dat' using 1:2 with points pt 7 title column(3), 'latest-3.dat' using 1:2 with points pt 7 title column(3)
