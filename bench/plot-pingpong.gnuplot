if (format eq 'eps') {
  set terminal postscript eps enhanced size 3.2,1.4 font 'Times'
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
}
gen_title(i) = word(title,i)
round(x) = floor(x + .5)
invpow2(v) = log(v) / log(2)
sizefmt(v) = abs(invpow2(v)-round(invpow2(v))) < 1e-7 ? sizefmt_M(v) : ''
sizefmt_M(v) = invpow2(v) >= 20 ? sprintf('%dM', v / 1024**2) : sizefmt_K(v)
sizefmt_K(v) = invpow2(v) >= 10 ? sprintf('%dK', v / 1024**1) : sizefmt_0(v)
sizefmt_0(v) = sprintf('%d', v)
set style data linespoints
set output outfile
set grid y
set border 3
set tics out nomirror
set key top left

set xlabel 'Message Size'
set ylabel 'Throughput (Gbps)'
set xrange [0:*]
set yrange [0:*]
set macro
xtics = "('0' 0"
do for [i=6:32:2] {
  xtics = xtics . sprintf(",'%s' %d", sizefmt(2**i), i-5)
}
xtics = xtics.")"
set xtics @xtics
plot for [i=1:words(infile)] word(infile,i) using (invpow2($2)-5):(2*$4*$2*8/10**9) title gen_title(i)
