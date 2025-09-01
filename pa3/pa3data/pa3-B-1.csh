#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa3data/tmux-pa3-B.csh for window 1

set prog = "./pa3"

uname -a
cat /etc/os-release
make clean
make pa3

/bin/rm -f pa3.log pa3.pid f?.out
source pa3data/pa3-setup.csh
${prog} pa3data/pa3-startup.ini
echo "[ (1) type <Ctrl+d> to run "\""diff "\$"sol3 f3.out"\"" ]"; cat
diff $sol3 f3.out
echo "[ (1) type <Ctrl+d> to run "\""cat pa3.log"\"" ]"; cat
cat pa3.log
