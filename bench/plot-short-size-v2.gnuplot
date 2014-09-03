if (format eq 'eps') {
  set terminal postscript eps enhanced solid size 3.1,2.1 font 'Times'
  unset key
  gen_title(i) = ''
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
  gen_title(i) = word(title,i)
}
set style data linespoints
set style line 1 linecolor rgbcolor 'red'
set style line 2 linecolor rgbcolor 'red'
set style line 3 linecolor rgbcolor 'blue'
set style line 4 linecolor rgbcolor 'black'
set style line 5 linecolor rgbcolor 'black'
set output outfile
set grid y
set border 3
set tics out nomirror

fig(infile) = "< export MAX=\`tail -1 ".infile."|cut -f1\`; awk '//{if ($1=='$MAX'&&$3==1)print $0 }' ".infile.'| sort -nk2'
set xlabel 'Message Size'
set ylabel 'Goodput (Gbps)'
set xrange [0:*]
set yrange [0:*]
set xtics ('0' 0)
plot for [i=1:words(infile)] fig(word(infile,i)) using ($0+1):($2*$4*8/10**9):xticlabel(2) title gen_title(i) linestyle i
