#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab6data/tmux-lab6d.txt for window 1

set prog = "lab6d"
set port = "12345"

echo "running ./${prog} ${port} 30"
./${prog} ${port} 30
