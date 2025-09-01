#!/bin/tcsh -f
#
# This is a sub-tcsh-script run from lab12data/tmux-lab12b.csh for node :12002

set prog = "./lab12b"
while (! -x ${prog})
    echo "Waiting for ${prog} to get compiled..."
    sleep 1
end

${prog} lab12data/lab12-12002.ini
echo "[ (2) type <Ctrl+d> to run :12002 again ]"; cat
${prog} lab12data/lab12-12002.ini
