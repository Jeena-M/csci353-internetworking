#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab11data/tmux-lab11a.csh for node :12004

set prog = "./lab11a"

echo "[ (3) type <Ctrl+d> when you are ready to start node :12004 ]"; cat
while (! -x ${prog})
    echo "Waiting for ${prog} to get compiled..."
    sleep 1
end

${prog} lab11data/lab11-12004.ini
echo "[ (3) type <Ctrl+d> to proceed to display log files ]"; cat
more lab11data/*.log
