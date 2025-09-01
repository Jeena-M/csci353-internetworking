#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa2data/tmux-pa2-H.csh for window 2

source pa2data/pa2-setup.csh
echo "[ (2) type <Ctrl+d> to run "\""./pa2 -c 127.0.0.1 12345 textbooks-2-small.jpg out.1 textbooks-3-small.jpg out.2 extra/USCHelmet.JPG out.3"\"" ]"; cat
./pa2 -c 127.0.0.1 12345 textbooks-2-small.jpg out.1 textbooks-3-small.jpg out.2 extra/USCHelmet.JPG out.3
echo "[ (2) type <Ctrl+d> to run "\""kill "\`"cat pa2.pid"\`""\"" ]"; cat
kill `cat pa2.pid`
echo "[ (2) type <Ctrl+d> to run "\""ls -l out.?"\""]"; cat
ls -l out.?
echo "[ (2) type <Ctrl+d> to run "\""openssl md5 out.?"\""]"; cat
openssl md5 out.?
echo "[ (2) type <Ctrl+d> to run "\""lsof -i 4"\"" ]"; cat
lsof -i 4
