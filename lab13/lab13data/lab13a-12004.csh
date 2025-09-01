#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab13data/tmux-lab13a.csh for node :12004

set prog = "./lab13a"
while (! -x ${prog})
    echo "Waiting for ${prog} to get compiled..."
    sleep 1
end

${prog} lab13data/lab13-12004.ini
echo "[ (3) type <Ctrl+d> to run :12004 again ]"; cat
${prog} lab13data/lab13-12004.ini
