#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab13data/tmux-lab13a.csh for node :12000

set prog = "./lab13a"

uname -a
lsb_release -a
# make clean
make lab13a
${prog} lab13data/lab13-12000.ini
