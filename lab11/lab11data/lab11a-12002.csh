#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab11data/tmux-lab11a.csh for node :12002

set prog = "./lab11a"

echo "[ (2) type <Ctrl+d> when you are ready to start node :12002 ]"; cat
while (! -x ${prog})
    echo "Waiting for ${prog} to get compiled..."
    sleep 1
end
${prog} lab11data/lab11-12002.ini
echo "[ (2) type <Ctrl+d> to run :12002 again ]"; cat
${prog} lab11data/lab11-12002.ini
