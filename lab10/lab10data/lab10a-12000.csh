#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab10data/tmux-lab10a.csh for node :12000

set prog = "./lab10a"

uname -a
lsb_release -a
# make clean
make lab10a
${prog} lab10data/lab10-12000.ini; \
echo "[ (1) type <Ctrl+d> when you are ready to restart node :12000 ]"; cat
${prog} lab10data/lab10-12000.ini
