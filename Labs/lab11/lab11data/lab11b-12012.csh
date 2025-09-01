#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab11data/tmux-lab11b.csh for node :12012

set prog = "./lab11a"

${prog} lab11data/lab11-12012.ini
echo "[ (4) type <Ctrl+d> to proceed to display log files ]"; cat
more lab11data/*.log
