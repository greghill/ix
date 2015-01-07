if (format eq 'eps') {
  set terminal postscript eps enhanced solid size 3.1,2.1 font 'Times'
  unset key
  gen_title(i) = ''
} else {
  set terminal pngcairo size 1024,1024 lw 1 font 'Times'
  gen_title(i) = word(title,i)
}
set style data linespoints
set style line 1 linetype 1 linecolor rgbcolor 'black'
set style line 2 linetype 1 linecolor rgbcolor 'red'
set output outfile
set grid y
set border 3
set tics out nomirror

set xrange [0:3600]
set yrange [0:100]
set xlabel 'Throughput (RPS x 10^{3})'
set ylabel 'Power (W)'

system('awk '."'".'{if ($5!="read"||$14>500)next;print $0}'."'".' '.infile.'|./to-pareto.py 15 21 1,3 > tmp');
system('cut -d# -f2 tmp|sort -u|xargs -I{} -n1 awk '."'".'{if($1","$3=="{}")print}'."' ".infile.' > tmp2');
system('awk '."'".'{if ($5!="read"||$14>500)next;x=$1"_"$3;if(x!=prv)print"";prv=x;print $15,$21}'."'".' tmp2 > tmp3');

plot 'tmp3' using ($1/10**3):2 title '' linestyle 1, \
     'tmp'  using ($1/10**3):2 title '' linestyle 2

system("rm -f tmp tmp2 tmp3");
