#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc,char * argv[]){
	if(argc<3){
	printf("Usage: xargs arugment1 arugment 2\n");	
	exit(0);
	}
	char * arg[MAXARG];
	for(int i =1;i<argc;i++) arg[i-1]=argv[i];
	char arug[1000];
	int flag =1 ;

	while(flag){
	int cnt  = 0;int last_arg = 0;
	int argv_cnt = argc-1;
	char ch = 0;
	while(1){
		flag = read(0,&ch,1);
		if(flag==0) exit(0);
		if(ch==' '||ch=='\n'){
			arug[cnt++]=0;
			arg[argv_cnt++]=&arug[last_arg];	
			last_arg = cnt;
			if(ch=='\n') break;
		}else arug[cnt++] = ch;
	}
	arg[argv_cnt]=0;
	if(fork()==0){
		exec(arg[0],arg);
	}else {
		wait(0);

	}
	

	}	
	exit(0);

}
