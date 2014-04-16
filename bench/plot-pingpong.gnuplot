if (format eq 'eps') {
  set terminal postscript eps enhanced lw 1 font 'Times'
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
set tics out nomirror font ',10'

set xlabel 'Message Size' font ',24'
set ylabel 'Throughput (Gbps)' font ',24'
set xrange [0:*]
set yrange [0:*]
set xtics ('0' 0)
plot for [i=1:words(infile)] word(infile,i) using (invpow2($2)-5):(2*$4*$2*8/10**9):xticlabel(sizefmt(column(2))) title gen_title(i)
