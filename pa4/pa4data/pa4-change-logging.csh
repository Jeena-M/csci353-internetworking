#!/bin/tcsh -f
#
# This file should be in the "pa4data" subdirectory of the directory where you have your "pa4" executable.  To run it do:
#
#     chmod 755 ./pa4data/pa4-change-logging.csh
#     ./pa4data/pa4-change-logging.csh {SAYHELLO|LSUPDATE} {0|1}

set argc=0
foreach arg ( $argv )
   @ argc = $argc + 1
end
set num_args = $#argv

if ($num_args != 2) then
    echo "usage: ./pa4data/pa4-change-logging.csh {SAYHELLO|LSUPDATE} {0|1}"
    exit 1
endif

set key=$argv[1]
set val=$argv[2]

if ($key != "SAYHELLO" && $key != "LSUPDATE") then
    echo "usage: ./pa4data/pa4-change-logging.csh {SAYHELLO|LSUPDATE} {0|1}"
    exit 1
endif

if ($val != 0 && $val != 1) then
    echo "usage: ./pa4data/pa4-change-logging.csh {SAYHELLO|LSUPDATE} {0|1}"
    exit 1
endif

foreach f (pa4data/*.ini)
    sed -i "1,$$s/${key}=[01]/${key}=${val}/" $f
end

echo "All instances of ${key} in pa4data/*.ini has been set to ${val}."
