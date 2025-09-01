#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab12data/tmux-lab12b.csh for node :12000

set prog = "./lab12b"

uname -a
lsb_release -a
# make clean
make lab12b
${prog} lab12data/lab12-12000.ini
