#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa2data/tmux-pa2-B.csh for window 1

set prog = "./pa2"

uname -a
cat /etc/os-release
make clean
make pa2

/bin/rm -f pa2.log pa2.pid f?.out out.?
source pa2data/pa2-setup.csh

echo "[ (1) type <Ctrl+d> to run "\""./pa2 -c releases.ubuntu.com 80 16.04/ubuntu-16.04.6-desktop-i386.iso out.1"\"" ]"; cat
./pa2 -c releases.ubuntu.com 80 16.04/ubuntu-16.04.6-desktop-i386.iso out.1
