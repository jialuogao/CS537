gcc xcheck.c
a.out mm.img1
a.out InaccessibleDir
yes | cp Repairsave Repair
diff <(hexdump -C Repairsave) <(hexdump -C Repair)
a.out -r Repair
diff <(hexdump -C Repairsave) <(hexdump -C Repair)
/u/c/s/cs537-1/tests/p5/runtests -c
rm a.out -f
