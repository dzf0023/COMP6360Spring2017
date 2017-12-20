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
#include <math.h>
#include "config.h"

using namespace std;

#define TIME 1000*100

#define pi 3.14159265358979323846

int default_heart_rate = 75;
string sourceIP;
int sourcePort;
string sourceAddress;

struct sockaddr_in myaddr, remaddr;
int fdHub;
unsigned char buf[DATASIZE];
int recvLen;

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

string to_string(double i)
{
    stringstream ss;
    ss << i;
    return ss.str();
}


string getRealIP(string tux_name)
{
    string str;
    ifstream referenceStringFile(REAL_IP_FILE);
    if (!referenceStringFile)
    {
        cout<<"No real ip file found!"<<endl;
    }
    else
    {
        char line[100];
        while (referenceStringFile.getline(line, sizeof(line)))
        {
            stringstream stream(line);
            string item;
            int i=0;
            // cout<<"line: "<<line<<endl;
            string tuxName;
            while(getline(stream, item, ' '))
            {
                if (i==0)
                {
                    tuxName = item;
                }
                if (i==1)
                {
                    if (tuxName == tux_name)
                    {
                        return item;
                    }
                }
                i++;
            }
        }
    }
    return "0";
}

int init_setup()
{
    string str;
    int count = 0;

    ifstream referenceStringFile(MANET_FILE);

    if (!referenceStringFile)
    {
        cout<<"No reference string file found!"<<endl;
        return 0;
    }
    else
    {
        char line[100];
        while (referenceStringFile.getline(line, sizeof(line)))
        {
            int nodeNumber;
            stringstream stream(line);
            string item;
            int i = 0;
            while(getline(stream, item, ' '))
            {
                if (i == 1)
                {
                    nodeNumber = stoi(item);
                }
                if (i == 2)
                {
                    // IP
                    item.erase(item.end()-1); // remove ',' at the end of the 'tux78,' for example.
                    if (nodeNumber == SOURCE_NODE)
                    {
                        sourceIP = getRealIP(item);
                    }
                }
                if (i == 3)
                {
                    if (nodeNumber == SOURCE_NODE)
                    {
                        sourcePort = stoi(item);
                        sourceAddress = sourceIP +":"+ item;
                    }
                }
                
                i++;
            }
        }
    }
    return 0;
}

int init_net()
{
    if ((fdHub=socket(AF_INET, SOCK_DGRAM, 0))==-1)
        cout<<"socket created"<<endl;

    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(0);

    if (bind(fdHub, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return 0;
    }

    return 0;
}

int sender(string bufSendingString)
{
    memset((char *) &remaddr, 0, sizeof(remaddr));
    remaddr.sin_family = AF_INET;
    remaddr.sin_port = htons(sourcePort);
    if (inet_aton(sourceIP.c_str(), &remaddr.sin_addr)==0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    if (sendto(fdHub, bufSendingString.data(), bufSendingString.size(), 0, (struct sockaddr *)&remaddr, sizeof(remaddr))==-1) 
    {
        perror("sendto");
        //exit(1);
    }


    return 0;
}

double deg2rad(double deg) {
    return (deg * pi / 180);
}

double rad2deg(double rad) {
    return (rad * 180 / pi);
}

void* heartSensorThread(void* args)
{
    for(int i=0;;i++)
    {    
        int increaseRate = 1;
        srand(time(NULL)-clock());
        // cout<<"current time: "<<clock()<<endl;
        int increaseRateProbability = 50; // 50/100
        int changeProbability=10;

        if (rand()%100<=changeProbability)
        {
            if(rand()%100<=increaseRateProbability)
            {
                default_heart_rate++;
            }
            else
            {
                default_heart_rate--;
            }
        }
        if(default_heart_rate<70)
        {
            default_heart_rate=70;
        }
        if(default_heart_rate>80)
        {
            default_heart_rate=80;
        }

        //cout <<"heart rate: "<< default_heart_rate<<endl;
        string sendingString;
        sendingString = "heart:"+to_string(i)+","+to_string(default_heart_rate)+",";
        cout<<"sendingString: "<<sendingString<<endl;
        sender(sendingString);

        usleep(1000*1000);
    }
}

void* locationSensorThread(void* args)
{
    double x=31.0;
    double y=28.0;
    double speed = 0.5; // m per second
    double brng = 0;
    double distance = 0;
    double duration = 0.5; // second

    for (int i = 0;; i++)
    {
        srand(time(NULL));
        int randNum = rand()%20-10;
        brng = brng + randNum;
        if (brng<0)
        {
            brng = brng +360;
        }
        if (brng>=360)
        {
            brng = brng - 360;
        }
        distance = speed * duration;
        x = x + distance * sin(brng);
        y = y + distance * cos(brng);
        if (x<10)
        {
            x = 10;
        }
        if (x>150)
        {
            x = 150;
        }
        if (y<10)
        {
            y=10;
        }
        if (y>150)
        {
            y = 150;
        }

        string sendingString;
        sendingString = "location:"+to_string(i)+","+to_string(x)+":"+to_string(y)+",";
        cout<<"sendingString: "<<sendingString<<endl;
        sender(sendingString);

        usleep(1000*500);
    }

}


void* oxygenSensorThread(void* args)
{
    int startTime = time(NULL);
    for (int i = 0;; i++)
    {
        double remainingOxygen = 100.0 - (100.0/3600.0)*(time(NULL)-startTime);

        string sendingString;
        sendingString = "oxygen:"+to_string(i)+","+to_string(remainingOxygen)+",";
        cout<<"sendingString: "<<sendingString<<endl;
        sender(sendingString);

        usleep(2000*1000);
    }
    return 0;
}

// mg per m3, 10mg/m3
void* toxicSensorThread(void* args)
{
    double toxicLevel=0.0;
    for (int i = 0;; i++)
    {
        int increaseRate = 1;
        srand(time(NULL)-clock());
        // cout<<"current time: "<<clock()<<endl;
        int increaseRateProbability = 30; // 50/100
        int changeProbability=10;

        if (rand()%100<=changeProbability)
        {
            if(rand()%100<=increaseRateProbability)
            {
                toxicLevel = toxicLevel - 0.001 * (rand()%10);
            }
            else
            {
                toxicLevel = toxicLevel + 0.001 * (rand()%10);
            }
        }
        if(toxicLevel<0)
        {
            toxicLevel=0;
        }
        if(toxicLevel>10)
        {
            toxicLevel=10;
        }

        //cout <<"toxic level: "<< toxicLevel<<endl;
        string sendingString;
        sendingString = "toxic:"+to_string(i)+","+to_string(toxicLevel)+",";
        cout<<"sendingString: "<<sendingString<<endl;
        sender(sendingString);

        usleep(1000*250);
    }
    return 0;
}

int main()
{
    init_setup();
    init_net();

    pthread_t tids[4];
    pthread_create(&tids[0], NULL, heartSensorThread,NULL);
    pthread_create(&tids[1], NULL, locationSensorThread,NULL);
    pthread_create(&tids[2], NULL, oxygenSensorThread,NULL);
    pthread_create(&tids[3], NULL, toxicSensorThread,NULL);
    pthread_exit(NULL);

    return 0;
}
