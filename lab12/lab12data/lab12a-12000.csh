#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab12data/tmux-lab12a.csh for node :12000

set prog = "./lab12a"

uname -a
lsb_release -a
# make clean
make lab12a
${prog} lab12data/lab12-12000.ini
