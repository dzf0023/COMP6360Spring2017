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
string localAddress = "127.0.0.1";
string destinationAddress = "127.0.0.1:10013";
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
                
                
                i++;
            }
            //cout<<endl;
            nodes.push_back(myNode);
        }

    }
    return 0;
}


int main()
{
    init_setup();
    system("gcc myNode.cpp -o myNode -lstdc++ -lpthread -lm");
    for (int i = 0; i < nodes.size(); ++i)
    {
        int myNodeNumber = nodes.at(i).nodeNum;
        string myNodeNumStr = to_string(myNodeNumber);
        if ((myNodeNumber != SOURCE_NODE) && (myNodeNumber != DESTINATION_NODE))
        {
            string command = "gnome-terminal -e './myNode "+myNodeNumStr+"' -t 'Node "+myNodeNumStr+"'";
            system(command.c_str());
            //cout<<"gnome-terminal -e './myNode "+myNodeNumStr+"' -t 'Node "+myNodeNumStr+"'"<<endl;
        }
    }

    return 0;
}