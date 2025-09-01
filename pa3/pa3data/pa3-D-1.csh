#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa3data/tmux-pa3-D.csh for window 1

set prog = "./pa3"

uname -a
cat /etc/os-release
make clean
make pa3

/bin/rm -f pa3.log pa3.pid f?.out
source pa3data/pa3-setup.csh
${prog} pa3data/pa3-startup.ini
echo "[ (1) type <Ctrl+d> to run a bunch of "\""diff"\"" commands ]"; cat
diff $sol8 dir2/127.0.0.1:12345/$f8
diff $sol1 dir2/127.0.0.1:12345/$f1
diff $sol0 dir2/127.0.0.1:12345/$f0
diff $sol4 dir2/127.0.0.1:12345/$f4
diff $sol8 dir3/127.0.0.1:12345/$f8
diff $sol1 dir3/127.0.0.1:12345/$f1
diff $sol0 dir3/127.0.0.1:12345/$f0
diff $sol4 dir3/127.0.0.1:12345/$f4
echo "[ (1) type <Ctrl+d> to run "\""cat pa3.log"\"" ]"; cat
cat pa3.log
