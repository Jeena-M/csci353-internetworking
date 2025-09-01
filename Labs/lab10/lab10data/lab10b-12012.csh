#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab10data/tmux-lab10b.csh for node :12012

set prog = "./lab10a"

${prog} lab10data/lab10-12012.ini
echo "[ (4) type <Ctrl+d> to proceed to display log files ]"; cat
more lab10data/*.log
