#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab6data/tmux-lab6c.txt for window 1

set prog = "lab6c"
set port = "12345"

uname -a
cat /etc/os-release
# make clean
make ${prog}

echo "running ./${prog} ${port} 10"
./${prog} ${port} 10
