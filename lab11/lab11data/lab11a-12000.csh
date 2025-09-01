#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab11data/tmux-lab11a.csh for node :12000

set prog = "./lab11a"

uname -a
lsb_release -a
# make clean
make lab11a
${prog} lab11data/lab11-12000.ini
