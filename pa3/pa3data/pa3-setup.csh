                # if you are running on nunki.usc.edu, you need to change "12345"
                #     in the next line to a port number assigned to you
                set portnum=12345
                set hostport="127.0.0.1:${portnum}"
                #
                # please understand that the grader can use one of his/her
                #     port numbers, so you code must work with ANY valid
                #     port number and not just 12345 or the ones assigned to you
                #     (your code must NOT explicit check if a valid port number is used)

                set datadir=pa2data

                set f0=extra/usctommy-photo.jpg
                set f1=viterbi-seal-rev-770x360.png
                set f2=upc_map.gif
                set f3=usc-seal-1597x360.png
                set f4=usctommy.gif
                set f5=extra/ufbm.pdf
                set f6=persistent-4.html
                set f7=extra/USC-Graphic-Identity-Reference-2017.pdf
                set f8=extra/rdt.pdf
                set f9=hamlet.txt

                set url0=http://$hostport/$f0
                set url1=http://$hostport/$f1
                set url2=http://$hostport/$f2
                set url3=http://$hostport/$f3
                set url4=http://$hostport/$f4
                set url5=http://$hostport/$f5
                set url6=http://$hostport/$f6
                set url7=http://$hostport/$f7
                set url8=http://$hostport/$f8
                set url9=http://$hostport/$f9

                set sol0=$datadir/$f0
                set sol1=$datadir/$f1
                set sol2=$datadir/$f2
                set sol3=$datadir/$f3
                set sol4=$datadir/$f4
                set sol5=$datadir/$f5
                set sol6=$datadir/$f6
                set sol7=$datadir/$f7
                set sol8=$datadir/$f8
                set sol9=$datadir/$f9

