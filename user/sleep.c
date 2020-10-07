#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: sleep some integer\n");
    exit(1);
  }
  int number = atoi(argv[1]);
  sleep(number);
  exit(0);
}

