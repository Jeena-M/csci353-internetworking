#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from pa2data/tmux-pa2-D.csh for window 2

source pa2data/pa2-setup.csh
echo "[ (2) type <Ctrl+d> to run "\""wget -r "\$"url10"\"" ]"; cat
wget -r http://127.0.0.1:12345/persistent-2.html
echo "[ (2) type <Ctrl+d> to run "\""kill "\`"cat pa2.pid"\`""\"" ]"; cat
kill `cat pa2.pid`
echo "[ (2) type <Ctrl+d> to run "\""ls -l 127.0.0.1:12345"\""]"; cat
ls -l 127.0.0.1:12345
echo "[ (2) type <Ctrl+d> to run "\""openssl md5 "\$"f10"\""]"; cat
openssl md5 $f10
echo "[ (2) type <Ctrl+d> to run "\""ls -l 127.0.0.1:12345/extra"\""]"; cat
ls -l 127.0.0.1:12345/extra
echo "[ (2) type <Ctrl+d> to run "\""openssl md5 127.0.0.1:12345/extra/*"\""]"; cat
openssl md5 127.0.0.1:12345/extra/*
echo "[ (2) type <Ctrl+d> to run "\""lsof -i 4"\"" ]"; cat
lsof -i 4
