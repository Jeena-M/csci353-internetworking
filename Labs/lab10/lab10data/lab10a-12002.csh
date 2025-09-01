#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab10data/tmux-lab10a.csh for node :12002

set prog = "./lab10a"

echo "[ (2) type <Ctrl+d> when you are ready to start node :12002 ]"; cat; \
while (! -x ${prog})
    echo "Waiting for ${prog} to get compiled..."
    sleep 1
end
${prog} lab10data/lab10-12002.ini; \
