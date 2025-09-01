#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa2data/tmux-pa2-B.csh for window 1

set prog = "./pa2"

uname -a
cat /etc/os-release
make clean
make pa2

/bin/rm -rf pa2.log pa2.pid f?.out out.? 127.0.0.1:12345
source pa2data/pa2-setup.csh

echo "[ (1) type <Ctrl+d> to run "\""pa2 -c merlot.usc.edu 80 index.html out.1 ..."\"" ]"; cat
./pa2 -c merlot.usc.edu 80 index.html out.1 cs353-s21/images/textbooks-3-small.jpg out.2
echo "[ (1) type <Ctrl+d> to run "\""ls -l out.1 out.2"\"" ]"; cat
ls -l out.1 out.2
echo "[ (1) type <Ctrl+d> to run "\""openssl md5 out.1 out.2"\"" ]"; cat
openssl md5 out.1 out.2
