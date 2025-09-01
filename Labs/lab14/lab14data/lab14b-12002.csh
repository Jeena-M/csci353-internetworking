#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab14data/tmux-lab14b.csh for node :12002

set prog = "./lab14b"
while (! -x ${prog})
    echo "Waiting for ${prog} to get compiled..."
    sleep 1
end

${prog} lab14data/lab14-12002.ini
echo "[ (2) type <Ctrl+d> to run :12002 again ]"; cat
${prog} lab14data/lab14-12002.ini
