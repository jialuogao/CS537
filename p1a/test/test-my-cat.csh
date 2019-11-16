#! /bin/csh -f
set TEST_HOME = /afs/cs.wisc.edu/u/c/s/cs537-1/tests/p1a
set source_file = my-cat.c
set binary_file = my-cat
set bin_dir = ${TEST_HOME}/bin
set test_dir = ${TEST_HOME}/tests-my-cat
${bin_dir}/generic-tester.py -s $source_file -b $binary_file -t $test_dir $argv[*]
