#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define FIFO "fifoOxygenLevel"

int main(int argc,char** argv)
{
    char buf_r[100];
    int  fd;
    int  nread;
    
    /* 创建管道 */
    if((mkfifo(FIFO,S_IRUSR|S_IWUSR)<0)&&(errno!=EEXIST))
        printf("cannot create fifo\n");
    
    printf("Preparing for reading bytes...\n");
    
    memset(buf_r,0,sizeof(buf_r));
    
    while(1)
    {
            /* 打开管道 */
        fd=open(FIFO,O_RDONLY,0);
        if(fd==-1)
        {
            perror("open");
            exit(1);    
        }
        memset(buf_r,0,sizeof(buf_r));
        nread=read(fd,buf_r,8);
        if(nread==-1)
        {
            if(errno==EAGAIN)
                printf("no data yet\n");
        }
    	else
    	{
            printf("Oxygen Level: %s \n",buf_r);
    	}
        close(fd);
        //sleep(1);
    }   
    pause(); /*暂停，等待信号*/
    unlink(FIFO); //删除文件
    return 0;
}

