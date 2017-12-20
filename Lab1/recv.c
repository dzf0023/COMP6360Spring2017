#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define DATASIZE 2048
#define PORT 10130
#define FIFO_OXYGEN "fifoOxygenLevel"
#define FIFO_LOCATION "fifoLocation"
#define FIFO_HEARTRATE "fifoHeartRate"

int main(int argc, char argv[])
{

    struct sockaddr_in myAddress;
    struct sockaddr_in remoteAddress;

    socklen_t addressLength = sizeof(remoteAddress);
    int recvLen;
    int fd;
    int msgcnt = 0;
    unsigned char buf[DATASIZE];
    unsigned char buf2[DATASIZE];

    /* create a UDP socket */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("Cannot create socket.\n");
        return 0;
    }

    // bind the socket to any valid IP address and a specific port
    memset((char *)&myAddress, 0, sizeof(myAddress));
    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddress.sin_port = htons(PORT);
    if (bind(fd, (struct sockaddr *)&myAddress, sizeof(myAddress)) < 0) {
        perror("bind failed");
        return 0;
    }

    int fdFifoLocation;
    int fdFifoOxygen;
    int fdFifoHeartRate;
    char bufFifo[DATASIZE];
    int nwrite;

    fdFifoLocation = open(FIFO_LOCATION, O_RDWR|O_CREAT|O_NONBLOCK,0777);
    fdFifoOxygen = open(FIFO_OXYGEN, O_RDWR|O_CREAT|O_NONBLOCK,0777);
    fdFifoHeartRate = open(FIFO_HEARTRATE, O_RDWR|O_CREAT|O_NONBLOCK,0777);


    int i=0;
    for(i=0;;i++)
    {
        recvLen = recvfrom(fd, buf, DATASIZE, 0, (struct sockaddr *)&remoteAddress, &addressLength);
        char *tag;
        if(recvLen > 0)
        {
            buf[recvLen] = 0;

            char *t;
            char *bpm = strtok(buf,",");
            int key=1;
            char *airPackRemaining;
            char *locationLongitude;
            char *locationAltitude;
            
            while(t=strtok(NULL, ","))
            {
                if(key==1)
                {
                    airPackRemaining=t;
                }
                if(key==2)
                {
                    locationLongitude=t;
                }
                if(key==3)
                {
                    locationAltitude=t;
                }
                if(key==4)
                {
                    tag=t;
                }
                key++;
            }
            int TAG = tag[0];

            printf("%s BPM, Oxygen %s%% remaining, location: (%s,%s)\n",bpm,airPackRemaining,locationLongitude,locationAltitude );



            if ((nwrite = write(fdFifoHeartRate,bpm,sizeof(bpm)))==-1)
            {
                printf("%d\n", errno);
            }
            else
            {
                //printf("Write %s to the FIFO, size of bpm: %d\n", bpm,sizeof(bpm));
            }

            if ((nwrite = write(fdFifoOxygen,airPackRemaining,sizeof(airPackRemaining)))==-1)
            {
                printf("%d\n", errno);
            }
            else
            {
                //printf("Write %s to the FIFO, size of air: %d\n", airPackRemaining, sizeof(airPackRemaining));
            }

            char * location;
            location = (char *)malloc(1024);
            //location = strcat(locationLongitude,",");
            //location = strcat(strcat(locationLongitude,","),locationAltitude);
            location = strcat(location, locationLongitude);
            location = strcat(location, ",");
            location = strcat(location, locationAltitude);

            // printf("2\n");
            // sprintf(location, "(%s,%s)",locationLongitude,locationAltitude);

            // printf("1\n");
            if ((nwrite = write(fdFifoLocation,location,30))==-1)
            {
                printf("%d\n", errno);
            }
            else
            {
                //printf("Write %s to the FIFO, size of location: %d\n", location, sizeof(location));
            }
            
            // char returnBuf[1];
            // returnBuf[0]=tag[0];

            sprintf(buf, "ack %s ", tag);
        }

        if (sendto(fd, buf,strlen(buf), 0, (struct sockaddr *)&remoteAddress, addressLength) < 0)
            perror("sendto");

        
    }
    return 0;
}
