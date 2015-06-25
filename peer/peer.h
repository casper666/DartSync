/*
* Modified: Jamie Yang 5/20/15
*/

// -------- header files
#include "../common/common.h"
#include "../common/constants.h"

// -------- constants

// -------- structure definitions

//Peer-side peer table.
typedef struct _peer_side_peer_t {
	// Remote peer IP address, 16 bytes.
	char ip[IP_LEN];
	//Current downloading file name.
	char file_name[FILE_NAME_LEN];
	//Timestamp of current downloading file.
	unsigned long file_time_stamp;
	//TCP connection to this remote peer.
	int sockfd;
	//Pointer to the next peer, linked list.
	struct _peer_side_peer_t *next;
} peer_peer_t;

//Each file can be represented as a node in file table
typedef struct node{
	//the size of the file
	int size;
	//the name of the file
	char *name;
	//the timestamp when the file is modified or created
	unsigned long int timestamp;
	//pointer to build the linked list
	struct node *pNext;
	//for the file table on peers, it is the ip address of the peer
	//for the file table on tracker, it records the ip of all peers which has the
	//newest edition of the file
	char *newpeerip;
}Node,*pNode;

// -------- function prototypes


// Adds node to download table
int PTPCreateDownloadEntry(peer_peer_t *table, peer_peer_t *node);

// Checks to see if file is being downloaded
int PTPCheckDLEntry(peer_peer_t table, char *file_name, int sockfd);

void* alive(void* arg);
int getMyIP(char *IPholder);
int PTTConnOpen();
int PTTConnClose(int conn);
int PeerDownloadTableInit();
void peerStop();
