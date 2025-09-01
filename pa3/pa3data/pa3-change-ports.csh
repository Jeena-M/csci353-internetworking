#!/bin/tcsh -f
#
# If you have to run your programs on a shared server such as viterbi-scf1.usc.edu,
#     you must not use 12345 as your port number and you must use a port number
#     that was assigned to you at the beginning of the semester.  If you don't
#     know your base port number, please contact the instructor.
#
# This file should be in the "pa3data" subdirectory of the directory where you have your PA3 executables.  To run it do:
#
#         chmod 755 ./pa3data/pa3-change-ports.csh
#         ./pa3data/pa3-change-ports.csh YOURPORT
#
# Please note that this command can ONLY be run against the ORIGINAL configuration files!
#     If you made a mistake the first time you ran it, you must wipe out the content of the
#     entire "pa3data" subdirectory by running the following command before running this command again:
#
#         tar xvf pa3data.tar.gz

set pa=pa3
set dir=${pa}data

set argc=0
foreach arg ( $argv )
   @ argc = $argc + 1
end
set num_args = $#argv

if ($num_args != 1) then
    echo "usage: ./${dir}/${pa}-change-ports.csh PORT"
    exit 1
endif

set baseport=$argv[1]

set rc = `echo ${baseport} | bc`
if ($rc == 0) then
    echo "usage: ./${dir}/change-ports.csh PORT"
    echo "       PORT must be a number"
    exit 1
endif

if (${baseport} < 10000 || ${baseport} > 64000) then
    echo "usage: ./${dir}/change-ports.csh PORT"
    echo "       PORT must be >= 10000 and <= 64000"
    exit 1
endif

set rc = `grep 12345 ${dir}/pa3-startup.ini | wc -l`
if ($rc == 0) then
    echo "Looks like you have already run this program once before."
    echo "You must wipe out the content of the entire "\""${dir}"\"" subdirectory (by running the following command) before you can run this program again:"
    echo "       tar xvf ${dir}.tar.gz"
    exit 1
endif

foreach f (${dir}/pa3-D-1.csh ${dir}/pa3-D-1.csh ${dir}/pa3-setup.csh ${dir}/pa3.ini ${dir}/pa3-startup-cout.ini  ${dir}/pa3-startup.ini)
    set port=${baseport}
    sed -i "1,$$s/12345/${port}/g" $f
end

echo "Done.  (Please be reminded that this program can only be run against the original configuration files.)"
