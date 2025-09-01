#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab10data/tmux-lab10b.csh for node :12002

set prog = "./lab10a"

${prog} lab10data/lab10-12002.ini
echo "[ (2) type <Ctrl+d> to run :12002 again ]"; cat
${prog} lab10data/lab10-12002.ini
