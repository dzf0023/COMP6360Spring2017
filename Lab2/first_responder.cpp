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

// #define NODE 1
#define TIME 1000*1000
#define PACKAGE_GENERATE_TIME 40*1000
#define PACKAGE_SEND_TIME 40*1000

typedef struct{
    int nodeNum;
    string IP;
    int port;
    int x;
    int y;
    vector <int> links;
} node;

vector <node> nodes;
vector <string> packageTable;
int k=1;
int global_port = 0;
int sequence = 1;
int mode=0;
int default_ethernet_loss_probability = 0;
int default_manet_loss_probability = 0;
int ethernetLossProbability = default_ethernet_loss_probability; // default loss rate (0~100)
int manetLossProbability = default_manet_loss_probability;
struct sockaddr_in myAddress;
struct sockaddr_in priorAddress;
struct sockaddr_in terminalAddress;
struct sockaddr_in sourceAddress;
struct sockaddr_in nextAddress;
string localAddress;
string destination_IP;
int destination_port;

int thisX;
int thisY;

string localIP;
int localPort=10010;
int sequenceCache = 0;
int ackCache = 0;
int fdSend;
int fdRecv;
vector <int> broadcastList; // node numbers to broadcast

int heartRate = -1;
double location_x = 999;
double location_y = 999;
double oxygen = -1;
double toxic = -1;

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

string destinationAddress = destination_IP +":"+ to_string(destination_port);

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
                    if (myNode.nodeNum == SOURCE_NODE)
                    {
                        localIP = myNode.IP;
                    }
                    if (myNode.nodeNum == DESTINATION_NODE)
                    {
                        destination_IP = myNode.IP;
                    }
                    // cout<<myNode.IP<<endl;
                }
                if (i == 3)
                {
                    myNode.port = stoi(item);
                    if (myNode.nodeNum == SOURCE_NODE)
                    {
                        localPort = stoi(item);
                        localAddress = localIP +":"+ item;
                    }
                    if (myNode.nodeNum == DESTINATION_NODE)
                    {
                        destination_port = myNode.port;
                    }
                }
                if (i == 4) // x
                {
                    myNode.x = stoi(item);
                    if (myNode.nodeNum == SOURCE_NODE)
                    {
                        thisX = myNode.x;
                    }
                }
                if (i == 5) // y
                {
                    myNode.y = stoi(item);
                    if (myNode.nodeNum == SOURCE_NODE)
                    {
                        thisY = myNode.y;
                    }
                    // cout<<myNode.y<<endl;
                }
                if (i >6)
                {
                    myNode.links.push_back(stoi(item));
                    if (myNode.nodeNum == SOURCE_NODE)
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

int gremlin()
{
    int lossPacket = 0;
    srand(time(NULL));
    if((rand()%100<ethernetLossProbability) && (mode == 0))
    {
        return 1; //loss
    }
    if((rand()%100<manetLossProbability) && (mode == 1))
    {
        return 1; //loss
    }
    else
    {
        return 0;
    }
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

void* generatePackage(void* args)
{
    for(;;)
    {
        string bufString;
        // sequence:1,source:22.22.22.22:3333,destination:33.33.33.33:2222,previous:66.66.66.66:1001,data:heart:80
        // sequence:1,22.22.22.22:3333,33.33.33.33:2222,66.66.66.66:1001,heart:80,location:23.456789:34.2342342,air:98,toxic:1
        // bufString = "sequence:"+to_string(k)+","+localAddress+","+destinationAddress+","+localAddress+",heart:"+to_string(heartRate)
        //     +",location:"+to_string(location_x)+":"+to_string(location_y)+",air:"+to_string(oxygen)+",toxic:"+to_string(toxic);
        bufString = ","+localIP+":"+to_string(localPort)+","+destination_IP+":"+to_string(destination_port)+","+to_string(SOURCE_NODE)+",heart:"+to_string(heartRate)
            +",location:"+to_string(location_x)+":"+to_string(location_y)+",air:"+to_string(oxygen)+",toxic:"+to_string(toxic)+","+"mode:";
        packageTable.push_back(bufString);
        usleep(PACKAGE_GENERATE_TIME);
    }
}




// only the hub have this thread
void* threadCreatePackageAndBroadcast(void* args) // send // mode 1 //manet
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

    for (;; k++)
    {
        for (;;)
        {
            //cout<<"manet mode thread: "<<mode<<endl;
            if ((mode==0) || (manetLossProbability == 100))
            {
                if (manetLossProbability == 100)
                {
                    mode = 0;
                }
                usleep(1000*100);
            }
            else if(mode == 1)
            {
                break;
            }
        }
        // sequenceCache = sequence;
        // string bufString;
        // // sequence:1,source:22.22.22.22:3333,destination:33.33.33.33:2222,previous:66.66.66.66:1001,data:heart:80
        // // sequence:1,22.22.22.22:3333,33.33.33.33:2222,66.66.66.66:1001,heart:80,location:23.456789:34.2342342,air:98,toxic:1
        // // bufString = "sequence:"+to_string(k)+","+localAddress+","+destinationAddress+","+localAddress+",heart:"+to_string(heartRate)
        // //     +",location:"+to_string(location_x)+":"+to_string(location_y)+",air:"+to_string(oxygen)+",toxic:"+to_string(toxic);
        // bufString = ","+localAddress+":"+to_string(localPort)+","+destination_IP+":"+to_string(destination_port)+","+to_string(NODE)+",heart:"+to_string(heartRate)
        //     +",location:"+to_string(location_x)+":"+to_string(location_y)+",air:"+to_string(oxygen)+",toxic:"+to_string(toxic)+","+"mode:";
        // packageTable.push_back(bufString);
        string bufSendingString;
        bufSendingString = "sequence:"+to_string(k) + packageTable.at(0)+to_string(mode)+",";

        // cout<<"what's in the package table?"<<endl;
        // for (int ii = 0; ii < packageTable.size(); ii++)
        // {
        //     cout<<packageTable.at(ii)<<endl;
        // }
        // cout<<"==============================="<<endl;
                    

        //cout<<"Broadcasting package..." << endl;

        if(packageTable.size()>0)
        {

            // for (int i = 0; i < broadcastList.size(); i++)
            // {
            if (mode == 1)
            {
                /* code */
                
                for (int j = 0; j < nodes.size(); j++)
                {
                    // if (nodes.at(j).nodeNum == broadcastList.at(i))
                    // {
                        // cout<<nodes.at(j).IP<<":"<<nodes.at(j).port<<endl;
                        // string destinationAddress = nodes.at(j).IP+":"+to_string(nodes.at(j).port);
                        memset((char *) &remaddr, 0, sizeof(remaddr));
                        remaddr.sin_family = AF_INET;
                        remaddr.sin_port = htons(nodes.at(j).port);
                        if (inet_aton(nodes.at(j).IP.c_str(), &remaddr.sin_addr)==0) 
                        {
                            fprintf(stderr, "inet_aton() failed - mode 1\n");
                            //exit(1);
                        }

                        //cout<<bufString<<endl;
                        //sprintf(buf,"%s",(char *)bufString.c_str());
                        // sprintf(buf, "",)
                        //cout<<"sending package to '"+nodes.at(j).IP+":"+to_string(nodes.at(j).port)+"'"<<endl;
                        //cout<< bufSendingString<<endl;
                        //cout<<"mode 1 thread: mode:"<<mode<<endl;
                        // if (manetLossProbability == 100) // manet is on mode 1
                        // {
                        //     mode = 0;
                        // }
                        //cout<<"mode 1 after thread: mode:"<<mode<<endl;
                        if(gremlin() == 0)
                        {
                            if(fastAndSlowFadingLoss(thisX, thisY, nodes.at(j).x, nodes.at(j).y) == 0)
                            {
                                
                                    
                                if (sendto(fdHub, bufSendingString.data(), bufSendingString.size(), 0, (struct sockaddr *)&remaddr, sizeof(remaddr))==-1) 
                                {
                                    perror("sendto");
                                    //exit(1);
                                }
                                
                            }
                        }
                    // }
                }
                if(ethernetLossProbability!=100)
                {
                    mode = 0;
                }
            // }
            }
        }
        // sequence++;
        usleep(PACKAGE_SEND_TIME);

    }
    return 0;
}

void* threadHubReceivePackageOrACK(void* args) // receive
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

    if (bind(fdHubReceive, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return 0;
    }
    for (int i = 0;; i++)
    {
        //cout<<"Waiting or receiving packages..."<<endl;
        int recvLen = recvfrom(fdHubReceive, buf, DATASIZE, 0, (struct sockaddr *)&priorAddress, &addressLength);
        if (recvLen>0)
        {
            // cout<<"received package:"<<endl;
            // cout<<buf<<endl;
            stringstream stream((char *)buf);
            string item;
            string sub_item;
            int j=0;
            string packageType;
            int receivedACKsequence;
            while(getline(stream, item, ','))
            {
                if (j == 0)
                {
                    stringstream sub_stream(item);
                    int t=0;
                    while(getline(sub_stream, sub_item, ':'))
                    {
                        if (t==0)
                        {
                            packageType = sub_item;
                            //cout<<"package type:"<<packageType<<endl;
                        }
                        if (t==1)
                        {
                            receivedACKsequence = stoi(sub_item);
                        }
                        t++;
                    }
                }
                if (j==1)
                {
                    if (packageType == "ack")
                    {
                        //cout<<"receivedACKsequence:"<<receivedACKsequence<<endl;
                        if (ackCache < receivedACKsequence)
                        {
                            // cout<<"ackCache:"<<ackCache<<endl;
                            // cout<<item<<endl;
                            packageTable.erase(packageTable.begin());
                            ackCache = receivedACKsequence;
                            //cout<<"ack cache: "<<ackCache<<endl;
                            //cout<<"sequence cache: "<<k<<endl;
                            //sequence++;
                            // first time received ack
                        }
                        else
                        {
                            // already received ack, discard
                        }
                    }
                    if (packageType == "sequence")
                    {
                        //cout<<"package type is sequence, discard."<<endl;
                    }
                    if (packageType == "heart")
                    {
                        // cout<<"received heart rate: "<<stoi(item)<<endl;
                        heartRate = stoi(item);
                    }
                    if (packageType == "location")
                    {
                        // cout<<"received location: "<<stoi(item)<<endl;
                        stringstream sub_stream(item);
                        int t=0;
                        while(getline(sub_stream, sub_item, ':'))
                        {
                            if (t==0)
                            {
                                stringstream sub_item_stream(sub_item);
                                sub_item_stream >> location_x;  
                            }
                            if (t==1)
                            {
                                stringstream sub_item_stream(sub_item);
                                sub_item_stream >> location_y;  
                            }
                            t++;
                        }
                    }
                    if (packageType == "oxygen")
                    {
                        // cout<<"received oxygen level: "<<item<<endl;
                        stringstream item_stream(item);
                        item_stream >> oxygen;  
                    }
                    if (packageType == "toxic")
                    {
                        // cout<<"received toxic: "<<item<<endl;
                        stringstream item_stream(item);
                        item_stream >> toxic;
                    }
                }
                j++;
            }
        }
    }

    return 0;
}


void* threadEthernetSend(void* args) // sender from vector // mode 0 // ethernet mode
{
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

    for (;; k++)
    {
        for (;;)
        {   
            // if mode is 1 manet mode then
            //cout<<"ethernet send thread: "<<mode<<endl;
            if ((mode==1) || (ethernetLossProbability == 100))
            {
                if (ethernetLossProbability == 100)
                {
                    mode = 1;
                }
                usleep(1000*100);
            }
            else if(mode == 0)
            {
                break;
            }
        }
        // string bufString;
        // bufString = ","+localAddress+":"+to_string(localPort)+","+destination_IP+":"+to_string(destination_port)+","+to_string(NODE)+",heart:"+to_string(heartRate)
        //     +",location:"+to_string(location_x)+":"+to_string(location_y)+",air:"+to_string(oxygen)+",toxic:"+to_string(toxic)+",mode:";
        // packageTable.push_back(bufString);
        if(packageTable.size()>0)
        {
            string bufSendingString;
            bufSendingString = "sequence:"+to_string(k) + packageTable.at(0)+to_string(mode)+",";

            //cout<<"what's in the package table?"<<endl;
            //for (int ii = 0; ii < packageTable.size(); ii++)
            //{
            //    cout<<packageTable.at(ii)<<endl;
            //}
            //cout<<"==============================="<<endl;
                        

            // cout<<"Broadcasting package..." << endl;

            // cout<<nodes.at(j).IP<<":"<<nodes.at(j).port<<endl;
            // string destinationAddress = nodes.at(j).IP+":"+to_string(nodes.at(j).port);
            memset((char *) &remaddr, 0, sizeof(remaddr));
            remaddr.sin_family = AF_INET;
            remaddr.sin_port = htons(destination_port);
            if (inet_aton(destination_IP.c_str(), &remaddr.sin_addr)==0) 
            {
                fprintf(stderr, "inet_aton() failed\n");
                // exit(1);
            }
            // else
            // {
            //     cout<<"inet_aton() success! "<<endl;
            // }

            //cout<<bufString<<endl;
            //sprintf(buf,"%s",(char *)bufString.c_str());
            // sprintf(buf, "",)
            
            // cout<< bufSendingString<<endl;
            //cout<<"mode 0 thread: mode:"<<mode<<endl;
            // if (ethernetLossProbability == 100) // ethernet is on mode 0
            // {
            //     mode = 1;
            // }
            if (gremlin()==0)
            {
                if (mode == 0)
                {
                    
                    if (sendto(fdHub, bufSendingString.data(), bufSendingString.size(), 0, (struct sockaddr *)&remaddr, sizeof(remaddr))==-1) 
                    {
                        perror("sendto");
                        // exit(1);
                    }
                }
            }
            if(manetLossProbability!=100)
            {    
                mode = 1;
            }
        }
        // sequence++;
        usleep(PACKAGE_SEND_TIME);

    }
    return 0;
}

void* waitingCinThread(void* args)
{
    // mode 0
    // mode 1
    // gremlin 30  //(0~100)
    for (int i = 0; ; ++i)
    {
        string commandString;
        cout<<">>";
        // cin >> commandString;
        getline(cin, commandString);
        cout << commandString << endl;
        string item;
        stringstream stream(commandString);
        int t=0;
        string commandType;
        while(getline(stream, item, ' '))
        {
            if (t==0)
            {
                commandType = item;
                // cout<<commandType<<endl;
                if (commandType == "package_table")
                {
                    cout<<"what's in the package table?"<<endl;
                    for (int ii = 0; ii < packageTable.size(); ii++)
                    {
                        cout<<packageTable.at(ii)<<endl;
                    }
                    cout<<"====================================================="<<endl;
                }
            }
            if (t==1)
            {
                // cout<<item<<endl;
                if (commandType == "mode")
                {
                    if ((stoi(item)==0) || (stoi(item)==1))
                    {
                        mode = stoi(item);
                    }
                    else
                    {
                        cout<<"mode: args error"<<endl;
                    }
                }
                else if (commandType == "ethernet_loss_rate")
                {
                    if ((stoi(item)>=0) && (stoi(item)<=100))
                    {
                        ethernetLossProbability = stoi(item);
                    }
                    else
                    {
                        cout<<"gremlin: args error"<<endl;
                    }
                }
                else if (commandType == "manet_loss_rate")
                {
                    if ((stoi(item)>=0) && (stoi(item)<=100))
                    {
                        manetLossProbability = stoi(item);
                    }
                    else
                    {
                        cout<<"gremlin: args error"<<endl;
                    }
                }
            }
            t++;
        }
    }
}

// void* changeModeThread(void* args)
// {
//     // for (int i = 0;; i++)
//     // {
//     //     if (ethernetLossProbability == 100)
//     //     {
//     //         ethernetLossProbability = default_ethernet_loss_probability;
//     //         if (mode == 0)
//     //         {
//     //             mode = 1;
//     //         }
//     //         else
//     //         {
//     //             mode = 0;
//     //         }
//     //     }
//     //     if (manetLossProbability == 100)
//     //     {
//     //         manetLossProbability = default_manet_loss_probability;
//     //         if (mode == 0)
//     //         {
//     //             mode = 1;
//     //         }
//     //         else
//     //         {
//     //             mode = 0;
//     //         }
//     //     }
//     //     usleep(1000*1000);
//     // }
// }

// void* modeChangingThread(void* args)
// {
//     for (int i = 0;; i++)
//     {
//         if (ethernetLossProbability == 100)
//         {
//             mode = 1;
//         }
//         if (manetLossProbability == 100)
//         {
//             mode = 0;
//         }
//         else
//         {
//             if (mode==1)
//             {
//                 mode = 0;
//             }
//             else if (mode == 0)
//             {
//                 mode = 1;
//             }
//         }
//         usleep(PACKAGE_SEND_TIME/10);
//     }
// }

int main()
{
    init_setup();
    cout<<"starting..."<<endl;
    cout<<5<<endl;
    usleep(1000*1000);
    cout<<4<<endl;
    usleep(1000*1000);
    cout<<3<<endl;
    usleep(1000*1000);
    cout<<2<<endl;
    usleep(1000*1000);
    cout<<1<<endl;
    usleep(1000*1000);
    cout<<0<<endl;
    pthread_t tids[5];
    pthread_create(&tids[0], NULL, waitingCinThread,NULL);
    pthread_create(&tids[1], NULL, threadEthernetSend,NULL);
    pthread_create(&tids[2], NULL, threadHubReceivePackageOrACK,NULL);
    pthread_create(&tids[3], NULL, threadCreatePackageAndBroadcast,NULL);
    // pthread_create(&tids[4], NULL, changeModeThread, NULL);
    pthread_create(&tids[4], NULL, generatePackage, NULL);
    //pthread_create(&tids[5], NULL, modeChangingThread, NULL);
    pthread_exit(NULL);

    return 0;
}
