#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
  int i;
  int len;
  char buf[512];
  char *xargv[MAXARG];
  for(int i=1;i<argc;i++){
    xargv[i-1]=argv[i];
  }// 拿到echo bye
  while(1){
    i=0;
    while(1){
      len=read(0,&buf[i],1);
      if(len<=0||buf[i]=='\n'){
        break;
      }
      i++;
    }
    if(i==0){
      break;
    }
      int pid=fork();
      if(pid>0){
        wait(0);
      }else{
        buf[i]=0;
        xargv[argc-1]=buf;// 将前一个命令的输出添加到命令行参数里变成echo bye hello too
        xargv[argc]=0;// 命令行参数必须以空字符结尾
        exec(xargv[0],xargv);
        exit(0);
      }
    }
  wait(0);
  exit(0);
}