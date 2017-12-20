#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <math.h>

#define DATASIZE 2048
#define MSGS 5	/* number of messages to send */
#define HEART_RATE_SENSOR_PORT	10130
#define LOCATION_SENSOR_PORT	10131
#define AIR_PACK_PORT			10132
#define LOSS_PROBABILITY		0.3
#define PORT 10130
#define pi 3.14159265358979323846

int heartRateSensor(int currentRate)
{
	int baseRateSit = 75;
	int baseRateWalk = 90;
	int baseRateRun = 130;

	int increaseRate = 1;
	srand(time(NULL));
	int increaseRateProbability = 50; // 50/100
	if(rand()%100<=increaseRateProbability)
	{
		increaseRate = 1;
	}
	else
	{
		increaseRate = -1;
	}
	currentRate = currentRate + increaseRate;
	if(currentRate<70)
	{
		currentRate=70;
	}
	if(currentRate>80)
	{
		currentRate=80;
	}

	return currentRate;
}

double airPackOxygenLevel(int startTime, int heartRate)
{
	double remainingOxygen = 100.0 - (100.0/3600.0)*(time(NULL)-startTime);
	/*
	if (heartRate>75)
	{
		return remainingOxygen - remainingOxygen*(0.1*(heartRate-75));
	}
	if (heartRate<75)
	{
		return remainingOxygen + remainingOxygen*(0.1*(75-heartRate));
	}*/
	return remainingOxygen;
}


double deg2rad(double deg) {
    return (deg * pi / 180);
}

double rad2deg(double rad) {
        return (rad * 180 / pi);
}


double * newLocation(double lon1, double lat1, double dist, int Brng)
{
    double lat2;
    double lon2;

    
    //lon1 = 32.575133;
    //lat1 = -85.499809;
    //printf("%lf, %lf\n",lon1, lat1);
    
    lon1 = deg2rad(lon1);
    lat1 = deg2rad(lat1);
    
    srand(time(NULL));
    int randNum = rand()%20-10;
    Brng = Brng + randNum;
	if (Brng<0)
	{
		Brng = Brng + 360;
	}
	if (Brng>=360)
	{
		Brng = Brng -360;
	}

    Brng = deg2rad(Brng);
    dist = (dist/6371.01);
    
    lat2 = asin( sin(lat1) * cos(dist) +
                cos(lat1) * sin(dist) * cos(Brng) );
    
    lon2 = lon1 + atan2(sin(Brng) * sin(dist) * cos(lat1),
                        cos(dist) - sin(lat1) * sin(lat2));
    
    //lon2 = fmod(lon2+3*pi, 2*pi)-pi;
    
    lat2 = rad2deg(lat2);
    lon2 = rad2deg(lon2);
    
    static double location[2];
    location[0] = lon2;
    location[1] = lat2;

    //printf("%lf, %lf\n",lon2, lat2);
    return location;
    //lon1 = 32.575133;
    //lat1 = -85.499809;
    //printf("%lf\n",distance(lon1, lat1, lon2, lat2, 'M'));
}

int gremlin()
{
	int lossPacket = 0;
	srand(time(NULL));
	int lossProbability = LOSS_PROBABILITY*100;
	if(rand()%100<=lossProbability)
	{
		return 1; //loss
	}
	else
	{
		return 0;
	}
}

int main(void)
{
	struct sockaddr_in myaddr, remaddr;
	int fd, i, slen=sizeof(remaddr);
	unsigned char buf[DATASIZE];
	unsigned char buf2[DATASIZE];
	unsigned char ACKbuf[DATASIZE];
	int recvlen;		/* # bytes in acknowledgement message */
	char *server = "127.0.0.1";	/* change this to use a different server */

	// create a socket

	if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
		printf("socket created\n");

	/* bind it to all local addresses and pick any port number */

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(0);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}       

	/* now define remaddr, the address to whom we want to send messages */
	/* For convenience, the host address is expressed as a numeric IP address */
	/* that we will convert to a binary format via inet_aton */

	memset((char *) &remaddr, 0, sizeof(remaddr));
	remaddr.sin_family = AF_INET;
	remaddr.sin_port = htons(PORT);
	if (inet_aton(server, &remaddr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	/* now let's send the messages */
	i = 0;
	int lossPacket=0;
	int heartRate = 75;
	double airRemaining = 100;
	double currentLon = 32.575133;
    double currentLat = -85.499809;
    int currentBrng = 0;
    double movingSpeed = 0.0005; // kilometres per second
	int startTime = time(NULL);
	//int tag = 0;
	for (;;) {
		if (lossPacket == 0)
        {
        	sleep(1);
        }
        int heartRate = heartRateSensor(heartRate);
        double oxygenRemaining = airPackOxygenLevel(startTime, heartRate);


        double * location = newLocation(currentLon,currentLat,movingSpeed, currentBrng);
        currentLon = location[0];
        currentLat = location[1];

		printf("Sending packet %d to %s port %d\n", i, server, PORT);
		int sendTime = time(NULL);
		sprintf(buf, "%d,%lf,%lf,%lf,%d", heartRate,oxygenRemaining,currentLon,currentLat, sendTime);
		sprintf(buf2,"nak %d",sendTime);
		sprintf(ACKbuf,"ack %d",sendTime);
		int lossPacket = 0;
		struct timeval timeout={1,0};//1s
    	int ret1=setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
    	int ret2=setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
		if (lossPacket = gremlin() == 0)
		{
			if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen)==-1) {
				perror("sendto");
				exit(1);
			}
		}
		else
        {
            printf("Packet %d loss, re-sending...\n", i);
            continue;
        }

		//receive an ack from the server

		recvlen = recvfrom(fd, buf2, strlen(buf2), 0, (struct sockaddr *)&remaddr, &slen);
			int tag = (int)buf2[0];
			//printf("tag == %s, sendTime== %d, recvlen==%d\n", buf2,sendTime,recvlen);

                if (recvlen >= 0 && strcmp(buf2, ACKbuf) == 0) 
                {
                	i++;
                    buf2[recvlen] = 0;	/* expect a printable string - terminate it */
                    printf("Received ACK: \"%s\"\n", buf2);
                    lossPacket=1;
                }
                else
                {
                	printf("Packet %d loss, re-sending...\n", i);
                }
        //printf("ret1=%d, ret2=%d\n",ret1,ret2);
	}
	close(fd);
	return 0;
}
