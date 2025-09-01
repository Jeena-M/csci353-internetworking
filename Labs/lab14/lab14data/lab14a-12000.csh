#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab14data/tmux-lab14a.csh for node :12000

set prog = "./lab14a"

uname -a
lsb_release -a
# make clean
make lab14a
${prog} lab14data/lab14-12000.ini
