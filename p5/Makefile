init: 
	gcc xcheck.c

test:
	/u/c/s/cs537-1/tests/p5/runtests -c

testec:
	a.out mm.img1
	a.out mm.img2
	a.out -r Repair

compare:
	diff <(hexdump -C Repairsave) <(hexdump -C Repair)

clean:
	rm -