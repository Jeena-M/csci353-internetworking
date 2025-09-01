#!/bin/tcsh -f

set datadir=pa1data

source $datadir/pa1-setup.csh

echo "(A) Type-I Maze ..."
                /bin/rm -f f?.out f??.out
                ./pa1 $p0 $datadir/f0 > f0.out
                ./pa1 $p1 $datadir/f1 > f1.out
                ./pa1 $p2 $datadir/f2 > f2.out
                ./pa1 $p3 $datadir/f3 > f3.out
                ./pa1 $p4 $datadir/f4 > f4.out
                ./pa1 $p5 $datadir/f5 > f5.out
                ./pa1 $p6 $datadir/f6 > f6.out
                ./pa1 $p7 $datadir/f7 > f7.out
                ./pa1 $p8 $datadir/f8 > f8.out
                ./pa1 $p9 $datadir/f9 > f9.out
                ./pa1 $p10 $datadir/f10 > f10.out
                ./pa1 $p11 $datadir/f11 > f11.out
                ./pa1 $p12 $datadir/f12 > f12.out
                ./pa1 $p13 $datadir/f13 > f13.out
                ./pa1 $p14 $datadir/f14 > f14.out
                ./pa1 $p15 $datadir/f15 > f15.out
                ./pa1 $p16 $datadir/f16 > f16.out
                ./pa1 $p17 $datadir/f17 > f17.out
                ./pa1 $p18 $datadir/f18 > f18.out
                ./pa1 $p19 $datadir/f19 > f19.out
                ./pa1 $p20 $datadir/f20 > f20.out
                ./pa1 $p21 $datadir/f21 > f21.out
                ./pa1 $p22 $datadir/f22 > f22.out
                ./pa1 $p23 $datadir/f23 > f23.out
                ./pa1 $p24 $datadir/f24 > f24.out
                foreach f (0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24)
                    echo "===> $datadir/sol/f$f"
                    diff $datadir/sol/f$f.out f$f.out
                end
                /bin/rm -f f?.out f??.out

