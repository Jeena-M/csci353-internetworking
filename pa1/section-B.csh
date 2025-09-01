#!/bin/tcsh -f

set datadir=pa1data

source $datadir/pa1-setup.csh

echo "(B) Type-II Maze ..."
                /bin/rm -f f?.out f??.out
                ./pa1 $p0 $datadir/f25 > f25.out
                ./pa1 $p1 $datadir/f26 > f26.out
                ./pa1 $p2 $datadir/f27 > f27.out
                ./pa1 $p3 $datadir/f28 > f28.out
                ./pa1 $p4 $datadir/f29 > f29.out
                ./pa1 $p5 $datadir/f30 > f30.out
                ./pa1 $p6 $datadir/f31 > f31.out
                ./pa1 $p7 $datadir/f32 > f32.out
                ./pa1 $p8 $datadir/f33 > f33.out
                ./pa1 $p9 $datadir/f34 > f34.out
                ./pa1 $p10 $datadir/f35 > f35.out
                ./pa1 $p11 $datadir/f36 > f36.out
                ./pa1 $p12 $datadir/f37 > f37.out
                ./pa1 $p13 $datadir/f38 > f38.out
                ./pa1 $p14 $datadir/f39 > f39.out
                ./pa1 $p15 $datadir/f40 > f40.out
                ./pa1 $p16 $datadir/f41 > f41.out
                ./pa1 $p17 $datadir/f42 > f42.out
                ./pa1 $p18 $datadir/f43 > f43.out
                ./pa1 $p19 $datadir/f44 > f44.out
                ./pa1 $p20 $datadir/f45 > f45.out
                ./pa1 $p21 $datadir/f46 > f46.out
                ./pa1 $p22 $datadir/f47 > f47.out
                ./pa1 $p23 $datadir/f48 > f48.out
                ./pa1 $p24 $datadir/f49 > f49.out
                foreach f (25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49)
                    echo "===> $datadir/sol/f$f"
                    diff $datadir/sol/f$f.out f$f.out
                end
                /bin/rm -f f?.out f??.out

