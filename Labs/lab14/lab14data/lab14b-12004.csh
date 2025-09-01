#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab14data/tmux-lab14b.csh for node :12004

set prog = "./lab14b"
while (! -x ${prog})
    echo "Waiting for ${prog} to get compiled..."
    sleep 1
end

${prog} lab14data/lab14-12004.ini
echo "[ (3) type <Ctrl+d> to proceed to display log files ]"; cat
more lab14data/*.log
