#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab14data/tmux-lab14b.csh for node :12000

set prog = "./lab14b"

uname -a
lsb_release -a
# make clean
make lab14b
${prog} lab14data/lab14-12000.ini
