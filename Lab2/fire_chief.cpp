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

int global_port = 0;
int sequence = 1;
struct sockaddr_in myAddress;
struct sockaddr_in priorAddress;
struct sockaddr_in terminalAddress;
struct sockaddr_in sourceAddress;
struct sockaddr_in nextAddress;
string sourceAddressString;
int mode=0;
string localAddress; // the localAddress will init in init_setup()
// string destinationAddress = "127.0.0.1:10013"; // useless
string localIP; // the localIP will init in init_setup()
string sourceIP;
int localPort;
int source_port;
int sequenceCache = 0;
int ackCache = 0;
int fdSend;
int fdRecv;
vector <int> broadcastList; // node numbers to broadcast
int thisX;
int thisY;
int heartRate = -1;
int location_x = 999;
int location_y = 999;
int oxygen = -1;
int toxic = -1;

string heart_display_IP;
string oxygen_display_IP;
string location_display_IP;
string toxic_display_IP;

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
                    if (myNode.nodeNum == DESTINATION_NODE)
                    {
                        localIP = myNode.IP;
                        heart_display_IP = localIP;
                        oxygen_display_IP=localIP;
                        location_display_IP=localIP;
                        toxic_display_IP=localIP;
                        cout<<"localIP: "<<localIP<<endl;
                    }
                    // cout<<myNode.IP<<endl;
                }
                if (i == 3)
                {
                    myNode.port = stoi(item);
                    if (myNode.nodeNum == DESTINATION_NODE)
                    {
                        localPort = stoi(item);
                        localAddress = localIP +":"+ item;
                    }
                }
                if (i == 4) // x
                {
                    myNode.x = stoi(item);
                    if (myNode.nodeNum == DESTINATION_NODE)
                    {
                        thisX = myNode.x;
                    }
                }
                if (i == 5) // y
                {
                    myNode.y = stoi(item);
                    if (myNode.nodeNum == DESTINATION_NODE)
                    {
                        thisY = myNode.y;
                    }
                    // cout<<myNode.y<<endl;
                }
                if (i >6)
                {
                    myNode.links.push_back(stoi(item));
                    if (myNode.nodeNum == DESTINATION_NODE)
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
void* threadNodeBroadcastPackageFromCache(void* args) //send
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
            string bufSendingString;
            bufSendingString = packageTable.at(0);
            //cout<<"bufSendingString: "<<endl;
            //cout<<bufSendingString<<endl;
            stringstream stream(bufSendingString);
            int t=0;
            string item;
            while(getline(stream, item, ','))
            {
                if (t==2)
                {
                    cout<<"destination address: "+item<<endl;
                    stringstream sub_stream(item);
                    int tt=0;
                    string sub_item;
                    while(getline(sub_stream, sub_item, ':'))
                    {
                        if (tt==0)
                        {
                            sourceIP = sub_item;
                            cout<<"IP: "<<sourceIP<<endl;
                        }
                        if (tt==1)
                        {
                            source_port = stoi(sub_item);
                            cout<<"Port: "<<source_port<<endl;
                        }
                        tt++;
                    }
                }
                if (t==4)
                {
                    stringstream sub_stream(item);
                    int tt=0;
                    string sub_item;
                    while(getline(sub_stream, sub_item, ':'))
                    {
                        if (tt==1)
                        {
                            cout<<"broadcast mode:"<<stoi(sub_item)<<endl;
                            mode = stoi(sub_item);
                        }
                        tt++;
                    }
                }
                t++;
            }
            packageTable.erase(packageTable.begin());
            //cout<<"mode:"<<mode<<endl;
            if (mode == 0)
            {
                memset((char *) &remaddr, 0, sizeof(remaddr));
                remaddr.sin_family = AF_INET;
                remaddr.sin_port = htons(source_port);
                //cout<<sourceIP<<endl;
                if (inet_aton(sourceIP.c_str(), &remaddr.sin_addr)==0) 
                {
                    fprintf(stderr, "inet_aton() failed 1.\n");
                    exit(1);
                }

                cout<<"sending package..."<< bufSendingString<<endl;
                if (sendto(fdHub, bufSendingString.data(), bufSendingString.size(), 0, (struct sockaddr *)&remaddr, sizeof(remaddr))==-1) 
                {
                    perror("sendto");
                    exit(1);
                }
            }
            else if (mode == 1)
            {
                // string bufString;
                // // sequence:1,source:22.22.22.22:3333,destination:33.33.33.33:2222,previous:66.66.66.66:1001,data:heart:80
                // // sequence:1,22.22.22.22:3333,33.33.33.33:2222,66.66.66.66:1001,heart:80,location:23.456789:34.2342342,air:98,toxic:1
                // bufString = "sequence:"+to_string(sequence)+","+localAddress+","+destinationAddress+","+localAddress+",heart:"+to_string(heartRate)
                //     +",location:"+to_string(location_x)+":"+to_string(location_y)+",air:"+to_string(oxygen)+",toxic:"+to_string(toxic);
                // packageTable.push_back(bufString);

                // cout<<"Broadcasting package..." << bufSendingString << endl;

                // for (int i = 0; i < broadcastList.size(); i++)
                // {
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
                                fprintf(stderr, "inet_aton() failed 2.\n");
                                exit(1);
                            }

                            //cout<<"sending package..."<< bufSendingString<<endl;
                            if(fastAndSlowFadingLoss(thisX, thisY, nodes.at(j).x, nodes.at(j).y) == 0)
                            {
                                if (sendto(fdHub, bufSendingString.data(), bufSendingString.size(), 0, (struct sockaddr *)&remaddr, sizeof(remaddr))==-1) 
                                {
                                    perror("sendto");
                                    exit(1);
                                }
                            }
                        // }
                    }
                // }
                //cout<<"received"<<endl;
            }
            //usleep(TIME);
        }
        else
        {
            //cout<<"Not yet received data..."<<endl;
            //usleep(TIME);
        }
    }
}

void* threadNodeReceivePackageOrACKThenForward(void* args) //receive and push ack package into cache and send data to display
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
    //cout<<"local port:"<<localPort<<endl;

    if (bind(fdHubReceive, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        cout<<"bind failed"<<endl;
        return 0;
    }





    struct sockaddr_in myaddr2, remaddr;
    int fdHub;
    
    int recvLen;

    if ((fdHub=socket(AF_INET, SOCK_DGRAM, 0))==-1)
        cout<<"socket created"<<endl;

    memset((char *)&myaddr2, 0, sizeof(myaddr2));
    myaddr2.sin_family = AF_INET;
    myaddr2.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr2.sin_port = htons(0);

    if (bind(fdHub, (struct sockaddr *)&myaddr2, sizeof(myaddr2)) < 0) {
        perror("bind failed");
        return 0;
    }






    for (int i = 0;; i++)
    {
        //cout<<"Waiting or receiving packages..."<<endl;
        string displayString;
        int recvLen = recvfrom(fdHubReceive, buf, sizeof(buf), 0, (struct sockaddr *)&priorAddress, &addressLength);
        if (recvLen>0)
        {
            cout<<"received string: "<<buf<<endl;
            stringstream stream((char *)buf);
            string item;
            string sub_item;
            int j=0;
            string packageType;
            string packageContent;
            string coutString;
            string forwardString;
            int isPush_back=0;
            int receivedSequence;
            int isDestination=0;
            while(getline(stream, item, ','))
            {
                if (j == 0)
                {
                    //sequence number
                    stringstream sub_stream(item);
                    int t=0;
                    packageContent = item + ",";   /////////////////
                    while(getline(sub_stream, sub_item, ':'))
                    {
                        if (t==0)
                        {
                            packageType = sub_item;
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
                    // source address
                    packageContent = packageContent + item+",";   ///////////////////
                    sourceAddressString = item;
                    if (packageType == "ack")
                    {
                        if (ackCache<receivedSequence)
                        {
                            isPush_back=1;
                            ackCache = receivedSequence;
                            // cout<<"ack cache: "<<ackCache<<endl;
                            // not yet received ack
                        }
                        else
                        {
                            // already received ack, discard
                        }
                    }
                    if (packageType == "sequence")
                    {
                        //cout << "sequenceCache: "<<sequenceCache<<endl;
                        //cout << "sequence: "<<receivedSequence<<endl;
                        if (sequenceCache<receivedSequence)
                        {
                            isPush_back=1;
                            sequenceCache = receivedSequence;
                            cout<<"sequence cache: "<<sequenceCache<<endl;
                        }
                        else
                        {
                            // discard
                        }
                    }
                }
                if (j==2)
                {
                    // destination address
                    if (item == localAddress)
                    {
                        isDestination = 1;
                        packageContent = "ack:"+to_string(receivedSequence)+ ","+localAddress+","+sourceAddressString+",";   /////////////////
                        // change the package type to ack
                    }
                    else
                    {
                        packageContent = packageContent + item;
                    }
                }
                if (j==3)
                {
                    // previous change
                    // previous hop
                    packageContent = packageContent + to_string(DESTINATION_NODE) + ",";
                }
                if (j==4 && packageType == "sequence")
                {
                    //heart rate
                    //cout << "Heart Rate"<< item<<"    ";
                    coutString = item;
                }
                if (j==5 && packageType == "sequence")
                {
                    // location
                    //cout << "Location: "<< item<<"    ";
                    coutString = coutString +","+item;
                    //packageContent = packageContent + ","+item;
                }
                if (j==6 && packageType == "sequence")
                {
                    //air
                    //cout << "Oxygen Level: "<< item<<"    ";
                    coutString = coutString +","+item;
                }
                if (j==7 && packageType == "sequence")
                {
                    // toxic
                    //cout << "Toxic Level: "<< item<<endl;
                    coutString = coutString +","+item+",";
                }
                if (j==8)
                {
                    //mode
                    stringstream sub_stream(item);
                    int t=0;
                    while(getline(sub_stream, sub_item, ':'))
                    {
                        if (t==1)
                        {
                            //cout<<"recv mode:"<<stoi(sub_item)<<endl;
                            if (mode != stoi(sub_item))
                            {
                                packageTable.clear();
                                mode = stoi(sub_item);
                            }
                        }
                        t++;
                    }
                    packageContent = packageContent + item+",";
                }
                j++;
            }

            //push_back
            if (isPush_back == 1)
            {
                // ready for forward packages
                cout<<"Pushing packet into packet table..." <<endl;
                cout<< packageContent << endl;

                cout<<" ------------------------------------------------------------------------------------"<<endl;
                cout<<"                                                                                     "<<endl;
                cout<<"        "<<coutString<<"        "<<endl;
                cout<<"                                                                                     "<<endl;
                cout<<" ------------------------------------------------------------------------------------"<<endl;

                //sendToDisplay(coutString);


                stringstream stream(coutString);
                int aa=0;
                string item;
                string destination_IP;
                int destination_port;
                while(getline(stream, item, ','))
                {
                    
                    if(aa==0)
                    {
                        destination_IP = heart_display_IP;
                        destination_port = heart_display_port;
                    }
                    if(aa==1)
                    {
                        destination_IP = location_display_IP;
                        destination_port = location_display_port;
                    }
                    if(aa==2)
                    {
                        destination_IP = oxygen_display_IP;
                        destination_port = oxygen_display_port;
                    }
                    if(aa==3)
                    {
                        destination_IP = toxic_display_IP;
                        destination_port = toxic_display_port;
                    }
                    if (aa<4)
                    {
                        // cout<<"destination_IP: "<<destination_IP<<endl;
                        // cout<<"destination_port: "<<destination_port<<endl;
                        memset((char *) &remaddr, 0, sizeof(remaddr));
                        remaddr.sin_family = AF_INET;
                        remaddr.sin_port = htons(destination_port);
                        if (inet_aton(destination_IP.c_str(), &remaddr.sin_addr)==0) 
                        {
                            fprintf(stderr, "inet_aton() failed\n");
                            exit(1);
                        }
                        string sendingItem = item+",";
                        //cout<<"display string: "<<sendingItem<<endl;
                        if (sendto(fdHub, sendingItem.data(), sendingItem.size(), 0, (struct sockaddr *)&remaddr, sizeof(remaddr))==-1) 
                        {
                            perror("sendto");
                            exit(1);
                        }
                    }
                    aa++;
                }

                packageTable.push_back(packageContent);
            }
            
        }
    }
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
