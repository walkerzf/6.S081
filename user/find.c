#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


void find(char * path, char *next,char * filename,int flag){
	char curpath[512];
	int fd;
	struct dirent de;
	struct stat st;
	strcpy(curpath,path);
	if(flag==0){
		  	int len = strlen(curpath);
			  	curpath[len] = '/';
				  	strcpy(curpath+len+1,next);
					 }
	 if((fd=open(curpath,0))<0){
		   	printf("find cannot open %s\n",curpath);
			  	return;
				 }
	  if(fstat(fd,&st)<0){
		    	printf("cannot stat\n");
			  	close(fd);
				  	return ;
					 }
	   switch(st.type){
		     	case T_DIR:
				  	while(read(fd,&de,sizeof(de))==sizeof(de)){
	if(de.inum==0) continue;
	if(strcmp(de.name,".")==0||strcmp(de.name,"..")==0) continue;
		find(curpath,de.name,filename,0);
	}
	break;
	case T_FILE:
	if(strcmp(filename,next)==0){
 	printf("%s\n", curpath);
	}
	break;
   }
	close(fd);
}

int 
main(int argc,char * argv[]){
	if(argc<3){
		printf("Usage find ..\n");
		exit(0);
	}
	char * path  = argv[1];
	char * filename = argv[2];
	find(path ,"",filename,1);
	exit(0);
}
