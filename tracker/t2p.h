/*
- t2p.h (tracker to peer AND peer to tracker)

This header file should contain all structures and functions for maintaining tracker to peer communication.

Specifically, the functions for encapuslating file information should be in this .h and .c.

*/

#ifndef T2P_H
#define T2P_H

#include "tracker.h" 


// ---------------------------------------------------------------
// Header files
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Structure definitions
// ---------------------------------------------------------------


// --------------------------------------------------------------- 
// Function prototpyes
// ---------------------------------------------------------------

// -------- header files

#include "../common/constants.h"
#include "../common/common.h"
// -------- constants



// -------- structure definitions

//T2P Downloading/Uploading File Message
typedef struct prt_hdr {
	unsigned int src_port;        //source port number
	unsigned int dest_port;       //destination port number
	unsigned int seq_num;         //sequence number of file used for partitioning
	unsigned short int length;    //total data length
	unsigned short int type;     //uploading == 1 or downloading == 2
} prt_hdr_t;

typedef struct partition {
	prt_hdr_t header;
	char data[MAX_DATA_LEN];
} partition_t;

// -------- function prototypes

/*
	Functions responsible for sending peer table information to peers and recieving peer package updates.
*/
int TTPSendPeerPkg(int peer_conn, ptp_tracker_t *data);

int PTTRecvPeerPkg(int peer_conn, ptp_peer_t *data);
/*
	Function responsible for sending file table over to the peer. 


*/
int TTPSendFileT(int peer_conn, ptp_tracker_t *data);

int PTTRecvFileT(int peer_conn, ptp_peer_t *data);

char* tableToCharArray(filetable_t* fileTable);

filetable_t* charArrayTotable(char* tableArray, int fileNum);

char* FileGetPartition(int length, int seq_num, char* file);


#endif

