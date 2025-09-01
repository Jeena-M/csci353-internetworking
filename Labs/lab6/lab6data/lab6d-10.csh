#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab6data/tmux-lab6d.txt for window 1

set prog = "lab6d"
set port = "12345"

uname -a
cat /etc/os-release
# make clean
make ${prog}

echo "running ./${prog} ${port} 10"
./${prog} ${port} 10
