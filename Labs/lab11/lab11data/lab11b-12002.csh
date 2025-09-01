#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab11data/tmux-lab11b.csh for node :12002

set prog = "./lab11a"

${prog} lab11data/lab11-12002.ini
echo "[ (2) type <Ctrl+d> to run :12002 again ]"; cat
${prog} lab11data/lab11-12002.ini
