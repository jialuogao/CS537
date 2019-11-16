#include "types.h"
#include "user.h"
#include "syscall.h"
#include "fs.h"
#include "traps.h"
#include "stat.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  printf(1, "there are %d file opened count\n", getopenedcount());
  printf(1, "there are %d file closed count\n", getclosedcount());
  
  int iter = atoi(argv[1]);
  printf(1, "the file will be open&closed for %d times\n", iter);

  for(int i = 0; i < iter; i++)
  {
    int f = open("testopen.cc", O_RDWR|O_CREATE);
    printf(1, "file opened %ds time\n", i);
    close(f);
    printf(1, "file closed %ds time\n", i);
  }
  printf(1, "there are %d file been opened\n", getopenedcount());
  printf(1, "there are %d file been opened\n", getclosedcount());
  exit();
}
