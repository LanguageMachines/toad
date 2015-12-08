#! /bin/sh
exe_dir=/home/sloot/usr/local/bin/
$exe_dir/makemblem -i $1 -o mblem.data
$exe_dir/timbl -a1 -w2 -f mblem.data -I mblem.tree
