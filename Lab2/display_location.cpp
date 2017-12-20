#include <stdlib.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <string.h>
#include <fstream>
#include <sstream>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <ctime>
#include "config.h"

using namespace std;


#define TIME 1000*100

int heartRate = -1;
int location_x = 999;
int location_y = 999;
int oxygen = -1;
int toxic = -1;

string heartRateString = "-1";
string locationString = "-1";
string oxygenString="-1";
string toxicString="-1";

int heart_display_port = 10136;
int oxygen_display_port = 10137;
int location_display_port = 10138;
int toxic_display_port = 10139;

int stoi(string args)
{
    return atoi(args.c_str());
}

string to_string(int i)
{
    stringstream ss;
    ss << i;
    return ss.str();
}

void* receiverThread(void* args)
{
    int fdHubReceive;
    unsigned char buf[DATASIZE];
    struct sockaddr_in myaddr, priorAddr;
    socklen_t addressLength = sizeof(priorAddr);
    if ((fdHubReceive = socket(AF_INET, SOCK_DGRAM, 0))==-1)
    {
        cout << "socket 'recv' created failed." << endl;
    }

    //bind
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(location_display_port);

    if (bind(fdHubReceive, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return 0;
    }
    for(;;)
    {
        int recvLen = recvfrom(fdHubReceive, buf, DATASIZE, 0, (struct sockaddr *)&priorAddr, &addressLength);
        if (recvLen>0)
        {
            stringstream stream((char *)buf);
            int i=0;
            string item;
            while(getline(stream, item, ','))
            {
                if(i==0)
                {
                    locationString = item;
                }
                i++;
            }
        }
    }
}

void* locationDisplayThread(void* args)
{
    for (int i = 0;; i++)
    {
        //time_t result = time(nullptr);
        //string currentTime;
        //cout<< asctime(localtime(&result));
        cout<<locationString<<endl;

        usleep(1000*1000);
    }
}

int main()
{
    pthread_t tids[2];
    pthread_create(&tids[0], NULL, receiverThread,NULL);
    pthread_create(&tids[1], NULL, locationDisplayThread,NULL);
    pthread_exit(NULL);
    return 0;
}