cpplint.py --extensions=c,h my-cat.c
cpplint.py --extensions=c,h my-sed.c
cpplint.py --extensions=c,h my-uniq.c

gcc -o my-cat my-cat.c -Wall -Werror
gcc -o my-sed my-sed.c -Wall -Werror
gcc -o my-uniq my-uniq.c -Wall -Werror

~cs537-1/tests/p1a/test-my-cat.csh
~cs537-1/tests/p1a/test-my-sed.csh
~cs537-1/tests/p1a/test-my-uniq.csh
