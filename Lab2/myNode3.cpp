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

#define NODE 3
#define TIME 10000
#define LOOP 4 

typedef struct{
    int nodeNum;
    string IP;
    int port;
    int x;
    int y;
    vector <int> links;
} node;

typedef struct{
    string packageContent;
    int previousNodeNumber;
} package_struct;

vector <node> nodes;
vector <package_struct> packageTable;
int thisX;
int thisY;
int global_port = 0;
int sequence = 1;
struct sockaddr_in myAddress;
struct sockaddr_in priorAddress;
struct sockaddr_in terminalAddress;
struct sockaddr_in sourceAddress;
struct sockaddr_in nextAddress;
string localAddress;
// string destinationAddress = "127.0.0.1:10013";
string previousAddressString;
string localIP;
int localPort;
int sequenceCache = 0;
int ackCache = 0;
int fdSend;
int fdRecv;
vector <int> broadcastList; // node numbers to broadcast

int heartRate = -1;
int location_x = 999;
int location_y = 999;
int oxygen = -1;
int toxic = -1;

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
                    // cout<<"tuxName:"<<tuxName<<"."<<endl;
                    // cout<<"tux_name:"<<tux_name<<"."<<endl;
                }
                if (i==1)
                {
                    // cout<<"tuxName:"<<tuxName<<"."<<endl;
                    // cout<<"tux_name:"<<tux_name<<"."<<endl;
                    // cout<<(tuxName==tux_name)<<endl;
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
            node myNode;
            // sscanf(line,
            //     "Node %d %[^,], %d %d %d links %[^*]",
            //     &myNode.nodeNum, myNode.IP, &myNode.port, &myNode.x, &myNode.y, myNode.links);
            stringstream stream(line);
            string item;
            int i = 0;
            while(getline(stream, item, ' '))
            {
                //cout<<item<<endl;
                if (i == 1)
                {
                    myNode.nodeNum = stoi(item);
                    // cout<<myNode.nodeNum<<endl;
                }
                if (i == 2)
                {
                     // IP
                    item.erase(item.end()-1); // remove ',' at the end of the 'tux78,' for example.
                    myNode.IP = getRealIP(item);
                    //myNode.IP = item;
                    // cout<<"real ip: "<<myNode.IP<<endl;
                    if (myNode.nodeNum == NODE)
                    {
                        localIP = myNode.IP;
                    }
                    // cout<<myNode.IP<<endl;
                }
                if (i == 3)
                {
                    myNode.port = stoi(item);
                    if (myNode.nodeNum == NODE)
                    {
                        localPort = stoi(item);
                        localAddress = localIP +":"+ item;
                    }
                }
                if (i == 4) // x
                {
                    myNode.x = stoi(item);
                    if (myNode.nodeNum == NODE)
                    {
                        thisX = myNode.x;
                    }
                }
                if (i == 5) // y
                {
                    myNode.y = stoi(item);
                    if (myNode.nodeNum == NODE)
                    {
                        thisY = myNode.y;
                    }
                    // cout<<myNode.y<<endl;
                }
                if (i >6)
                {
                    myNode.links.push_back(stoi(item));
                    if (myNode.nodeNum == NODE)
                    {
                        broadcastList.push_back(stoi(item));
                    }
                }
                i++;
            }
            //cout<<endl;
            nodes.push_back(myNode);
        }

    }
    return 0;
}

int gremlin(double p)
{
    int lossPacket = 0;
    srand(time(NULL));
    if(rand()%100<p)
    {
        return 1; //loss
    }
    else
    {
        return 0;
    }
}

int fastAndSlowFadingLoss(int x1, int y1, int x2, int y2)
{
    double distance = sqrt(abs(x1-x2)*abs(x1-x2) + abs(y1-y2)*abs(y1-y2));
    int p = 100;
    if (distance<100)
    {
        p = -0.05 * distance + 100 ;
    }
    else if((distance>=100) && (distance<=101))
    {
        p = -90 * distance +9090;
    }
    else if ((distance>101) && (distance<=200))
    {
        p = (-5.0/99.0) * distance + (1000.0/99.0);
    }
    else
    {
        p = 0;
    }

    return gremlin(100-p);
}


// only the hub have this thread
void* threadNodeBroadcastPackageFromCache(void* args) //send / forwarding
{
    // hub
    // first send
    // if received the ACK, then broadcast next package
    // if not received the ACK, then re-send the package

    struct sockaddr_in myaddr, remaddr;
    int fdHub;
    unsigned char buf[DATASIZE];
    int recvLen;
    char * server;

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

    /* now define remaddr, the address to whom we want to send messages */
    /* For convenience, the host address is expressed as a numeric IP address */
    /* that we will convert to a binary format via inet_aton */

    for (int k = 0;; k++)
    {
        if (packageTable.size()>0)
        {
            // string bufString;
            // // sequence:1,source:22.22.22.22:3333,destination:33.33.33.33:2222,previous:66.66.66.66:1001,data:heart:80
            // // sequence:1,22.22.22.22:3333,33.33.33.33:2222,66.66.66.66:1001,heart:80,location:23.456789:34.2342342,air:98,toxic:1
            // bufString = "sequence:"+to_string(sequence)+","+localAddress+","+destinationAddress+","+localAddress+",heart:"+to_string(heartRate)
            //     +",location:"+to_string(location_x)+":"+to_string(location_y)+",air:"+to_string(oxygen)+",toxic:"+to_string(toxic);
            // packageTable.push_back(bufString);
            string bufSendingString;
            int previousHopNumber;
            previousHopNumber = packageTable.at(0).previousNodeNumber;
            bufSendingString = packageTable.at(0).packageContent;

            // cout<<"what's in the package table?"<<endl;
            // for (int ii = 0; ii < packageTable.size(); ii++)
            // {
            //     cout<<packageTable.at(ii).packageContent<<endl;
            // }
            // cout<<"==============================="<<endl;

            // cout<<"Broadcasting package..." << endl;
            // cout<<"bufSendingString: "<<bufSendingString << endl;
            packageTable.erase(packageTable.begin());

            // for (int i = 0; i < broadcastList.size(); i++)
            // {
                for (int j = 0; j < nodes.size(); j++)
                {
                    if (nodes.at(j).nodeNum != previousHopNumber)
                    {
                        // cout<<nodes.at(j).IP<<":"<<nodes.at(j).port<<endl;
                        // string destinationAddress = nodes.at(j).IP+":"+to_string(nodes.at(j).port);
                        memset((char *) &remaddr, 0, sizeof(remaddr));
                        remaddr.sin_family = AF_INET;
                        remaddr.sin_port = htons(nodes.at(j).port);
                        if (inet_aton(nodes.at(j).IP.c_str(), &remaddr.sin_addr)==0) 
                        {
                            fprintf(stderr, "inet_aton() failed\n");
                            exit(1);
                        }

                        //cout<<bufString<<endl;
                        //sprintf(buf,"%s",(char *)bufString.c_str());
                        // sprintf(buf, "",)
                        // cout<<"sending package "<< bufSendingString<<endl;
                        // cout<<"     to hop "<<broadcastList.at(i)<<endl;
                        if(fastAndSlowFadingLoss(thisX, thisY, nodes.at(j).x, nodes.at(j).y) == 0)
                        {
                            if (sendto(fdHub, bufSendingString.data(), bufSendingString.size(), 0, (struct sockaddr *)&remaddr, sizeof(remaddr))==-1) 
                            {
                                perror("sendto");
                                exit(1);
                            }
                        }
                    }
                }
            // }
            // cout<<"received"<<endl;
            usleep(TIME);
        }
        else
        {
            //cout<<"Not yet received data..."<<endl;
            usleep(TIME);
        }
    }
    return 0;
}

void* threadNodeReceivePackageOrACKThenForward(void* args) //receive and push package into cache 
{

    int fdHubReceive;
    unsigned char buf[DATASIZE];
    struct sockaddr_in myaddr, priorAddr;
    socklen_t addressLength = sizeof(priorAddr);
    if ((fdHubReceive = socket(AF_INET, SOCK_DGRAM, 0))==-1)
    {
        cout << "socket 'send' created failed." << endl;
    }

    //bind
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(localPort);
    // cout<<"local port:"<<localPort<<endl;

    if (bind(fdHubReceive, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        cout<<"bind failed"<<endl;
        return 0;
    }
    for (int i = 0;; i++)
    {
        // cout<<"Waiting or receiving packages..."<<endl;
        int recvLen = recvfrom(fdHubReceive, buf, DATASIZE, 0, (struct sockaddr *)&priorAddress, &addressLength);
        if (recvLen>0)
        {
            // cout<<"recvLen: "<<recvLen<<endl;
            // cout<<buf<<endl;
            stringstream stream((char *)buf);
            string item;
            string sub_item;
            int j=0;
            string packageType;
            string packageContent;
            int isPush_back=0;
            int receivedSequence;
            while(getline(stream, item, ','))
            {
                if (j == 0)
                {
                    stringstream sub_stream(item);
                    int t=0;
                    packageContent = item + ",";
                    while(getline(sub_stream, sub_item, ':'))
                    {
                        if (t==0)
                        {
                            packageType = sub_item;
                            // cout<<"package tyep: "<<packageType<<endl;
                        }
                        if (t==1)
                        {
                            receivedSequence = stoi(sub_item);
                        }
                        t++;
                    }
                }
                if (j==1)
                {
                    //source
                    packageContent = packageContent + item+",";
                    // cout<<"@@@@@@@@"<<packageType<<":"<<item<<endl;
                    // cout<<"package type:"<<packageType<<endl;
                    // cout<<"ackCache: "<<ackCache<<endl;
                    // cout<<"sequenceCache: "<<sequenceCache<<endl;
                    // cout<<"receivedSequence: "<<receivedSequence<<endl;
                    if (packageType == "ack")
                    {
                        if (ackCache<receivedSequence)
                        {
                            isPush_back=1;
                            ackCache = receivedSequence;
                            // not yet received ack
                        }
                        else
                        {
                            // already received ack, discard
                        }
                    }
                    if (packageType == "sequence")
                    {
                        // cout << "sequenceCache: "<<sequenceCache<<endl;
                        // cout << "sequence: "<<receivedSequence<<endl;
                        if (sequenceCache<receivedSequence)
                        {
                            // cout<<item<<endl;
                            isPush_back=1;
                            sequenceCache = receivedSequence;
                        }
                        else
                        {
                            // discard
                        }
                    }
                }
                if (j==2)
                {
                    // destination
                    packageContent = packageContent + item + ",";
                }
                if (j==3)
                {
                    // previous hop number
                    // previous address will be changed
                    previousAddressString = item;
                    // cout<<"previousAddressString: "<<previousAddressString<<endl;
                    packageContent = packageContent + to_string(NODE);
                }
                if (j==4 && packageType == "sequence" )
                {
                    packageContent = packageContent + "," + item + ",";
                }
                if (j==5&& packageType == "sequence")
                {
                    packageContent = packageContent + item + ",";
                }
                if (j==6&& packageType == "sequence")
                {
                    packageContent = packageContent + item + ",";
                }
                if (j==7&& packageType == "sequence")
                {
                    packageContent = packageContent + item;
                }
                if (j==8 && packageType == "sequence")
                {
                    packageContent = packageContent + "," + item+",";
                }
                j++;
            }
            // cout<<"package content: "<<packageContent<<endl;
            // cout<<"isPush_back: "<<isPush_back<<endl;
            //push_back
            if (isPush_back == 1)
            {
                // ready for forward packages
                cout << "Pushing packet into cache table..." << endl;
                cout << packageContent << endl;
                package_struct myPackage;
                myPackage.packageContent = packageContent;
                myPackage.previousNodeNumber = stoi(previousAddressString);
                packageTable.push_back(myPackage);
            }

        }
    }

    return 0;
}

int main()
{
    init_setup();
    pthread_t tids[2];
    pthread_create(&tids[0], NULL, threadNodeReceivePackageOrACKThenForward,NULL);
    pthread_create(&tids[1], NULL, threadNodeBroadcastPackageFromCache,NULL);
    pthread_exit(NULL);

    return 0;
}