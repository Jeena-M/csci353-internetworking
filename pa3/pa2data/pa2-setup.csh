#!/bin/tcsh -f
                #
                # please understand that your code must work with any port number
                #     (i.e., your code must NOT explicit check if a particular port number is used)
                #
                set host=127.0.0.1
                set portnum=12345
                set hostport="${host}:${portnum}"
                #
                # please understand that your code must work with any homedir
                #     (i.e., your code must NOT hard-code the value of homedir)
                #
                set datadir=pa2data
                set soldir=$datadir/sol

                set sleeptime=1

                set f0=extra/usctommy-2.gif
                set f1=extra/early.png
                set f2=extra/usctommy-photo.jpg
                set f3=extra/large-photo.jpg
                set f4=extra/Late.PNG
                set f5=extra/binomial.pdf
                set f6=textbooks-2-small.jpg
                set f7=textbooks-3-small.jpg
                set f8=viterbi-seal-rev-770x360.png
                set f9=usctommy.gif
                set f10=${host}:$portnum/persistent-2.html

                set url0=http://${host}:$portnum/extra/usctommy-2.gif
                set url1=http://${host}:$portnum/extra/early.png
                set url2=http://${host}:$portnum/extra/usctommy-photo.jpg
                set url3=http://${host}:$portnum/extra/large-photo.jpg
                set url4=http://${host}:$portnum/extra/Late.PNG
                set url5=http://${host}:$portnum/extra/binomial.pdf
                set url6=http://${host}:$portnum/textbooks-2-small.jpg
                set url7=http://${host}:$portnum/textbooks-3-small.jpg
                set url8=http://${host}:$portnum/viterbi-seal-rev-770x360.png
                set url9=http://${host}:$portnum/usctommy.gif
                set url10=http://${host}:$portnum/persistent-2.html

                set url99=http://releases.ubuntu.com/16.04/ubuntu-16.04.6-desktop-i386.iso

