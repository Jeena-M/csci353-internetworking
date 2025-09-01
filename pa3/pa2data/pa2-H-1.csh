#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa2data/tmux-pa2-H.csh for window 1

set prog = "./pa2"

uname -a
cat /etc/os-release
make clean
make pa2

/bin/rm -rf pa2.log pa2.pid f?.out out.? 127.0.0.1:12345
source pa2data/pa2-setup.csh
echo "[ (1) Running "\""${prog} pa2data/pa2-startup-cout.ini"\"" ... ]"
${prog} pa2data/pa2-startup-cout.ini
