set terminal png
set output "perf.png"
set style data lines
set xlabel "Time"
set ylabel "FPS"
plot "perf.dat" using 2:1 title ""
