#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa2data/tmux-pa2-B.csh for window 1

set prog = "./pa2"

uname -a
cat /etc/os-release
make clean
make pa2

/bin/rm -f pa2.log pa2.pid f?.out
source pa2data/pa2-setup.csh
echo "[ (1) Running "\""${prog} pa2data/pa2-startup.ini"\"" ... ]"
${prog} pa2data/pa2-startup.ini
echo "[ (1) type <Ctrl+d> to run "\""cat pa2.log"\"" ]"; cat
cat pa2.log
