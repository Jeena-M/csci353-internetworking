#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab6data/tmux-lab6c.txt for window 1

set prog = "lab6c"
set port = "12345"

echo "running ./${prog} ${port} 50"
./${prog} ${port} 50
