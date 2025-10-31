set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "SNR (dB)"
set cblabel offset 3
unset key
set terminal png
set output "nr-rem-scheduler-snr.png"
set size ratio -1
set cbrange [-5:30]
set xrange [-50:50]
set yrange [-50:50]
set xtics font "Helvetica,17"
set ytics font "Helvetica,17"
set cbtics font "Helvetica,17"
set xlabel font "Helvetica,17"
set ylabel font "Helvetica,17"
set cblabel font "Helvetica,17"
plot "nr-rem-scheduler.out" using ($1):($2):($4) with image
set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "SINR (dB)"
set cblabel offset 3
unset key
set terminal png
set output "nr-rem-scheduler-sinr.png"
set size ratio -1
set cbrange [-5:30]
set xrange [-50:50]
set yrange [-50:50]
set xtics font "Helvetica,17"
set ytics font "Helvetica,17"
set cbtics font "Helvetica,17"
set xlabel font "Helvetica,17"
set ylabel font "Helvetica,17"
set cblabel font "Helvetica,17"
plot "nr-rem-scheduler.out" using ($1):($2):($5) with image
set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "IPSD (dBm)"
set cblabel offset 3
unset key
set terminal png
set output "nr-rem-scheduler-ipsd.png"
set size ratio -1
set cbrange [-100:-20]
set xrange [-50:50]
set yrange [-50:50]
set xtics font "Helvetica,17"
set ytics font "Helvetica,17"
set cbtics font "Helvetica,17"
set xlabel font "Helvetica,17"
set ylabel font "Helvetica,17"
set cblabel font "Helvetica,17"
plot "nr-rem-scheduler.out" using ($1):($2):($6) with image
set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "SIR (dB)"
set cblabel offset 3
unset key
set terminal png
set output "nr-rem-scheduler-sir.png"
set size ratio -1
set cbrange [-5:30]
set xrange [-50:50]
set yrange [-50:50]
set xtics font "Helvetica,17"
set ytics font "Helvetica,17"
set cbtics font "Helvetica,17"
set xlabel font "Helvetica,17"
set ylabel font "Helvetica,17"
set cblabel font "Helvetica,17"
plot "nr-rem-scheduler.out" using ($1):($2):($7) with image
