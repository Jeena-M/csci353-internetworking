#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab6data/tmux-lab6b.csh for window 1

set prog = "lab6b"
set port = "12345"

uname -a
cat /etc/os-release
# make clean
make ${prog}

echo "running ./${prog} ${port}"
./${prog} ${port}
