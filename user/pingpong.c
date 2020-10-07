#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
//The parent should send a byte to the child;
//the child should print "<pid>: received ping", where <pid> is its process ID, write the byte on the pipe to the parent, and exit; 
//the parent should read the byte from the child, print "<pid>: received pong", and exit. 

void
main(int argc, char *argv[])
{	
  
  int p1[2];
  int p2[2];
  pipe(p1);
  pipe(p2);
  char a[1] = {'a'};
  char msg = '0';
  if(fork()==0){
  	//child receive ping
  	read(p2[0],a,1);
  	printf("%d: received ping\n", getpid());
  	write(p1[1],&msg,1);
  	exit(0);
  }else{
  	//parent reveive pong
  	write(p2[1],&msg,1);
  	read(p1[0],a,1);
  	printf("%d: received pong\n", getpid());
  	exit(0);
  }
}
