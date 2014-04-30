if (format eq 'eps') {
  set terminal postscript eps enhanced solid size 3.2,1.4 font 'Times'
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
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'blue'
set style line 3 linecolor rgbcolor 'black'
set style line 4 linecolor rgbcolor 'black'
set style line 5 linecolor rgbcolor 'black'
set output outfile
set grid y
set border 3
set tics out nomirror
set key bottom right invert

set xlabel 'Message Size (KB)'
set ylabel 'Goodput (Gbps)'
set xrange [0:512]
set yrange [0:10]
plot for [i=1:words(infile)] word(infile,i) using ($2/1024):(2*$4*$2*8/10**9) title gen_title(i) linestyle i
