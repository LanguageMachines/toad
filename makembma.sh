#! /bin/sh
exe_dir=/home/sloot/usr/local/bin/
$exe_dir/makembma -i $1 -o mbma.data
$exe_dir/timbl -a1 -w2 -f mbma.data -I mbma.igtree
