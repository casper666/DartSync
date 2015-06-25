/*
* Modified: Jamie Yang 5/20, fho 5/20
*/
// ---------------------------------------------------------------
// Header files
// ---------------------------------------------------------------

#ifndef TRACKER_H
#define TRACKER_H

#include "../common/common.h"
#include "../common/constants.h"
#include "t2p.h"
#include "../filemonitor/filetable.h"

// ---------------------------------------------------------------
// Structure definitions
// ---------------------------------------------------------------

/* The tracker side peer table. (SLL) */
typedef struct _tracker_side_peer_t {
	//Remote peer IP address.
	struct in_addr ip;
	//Last alive timestamp of this peer.
	long last_time_stamp;
	//TCP connection to this remote peer.
	int sockfd;
	//ID for the handShakeMonitor() thread created for this particular connection.
	pthread_t thread_id;
	//Pointer to the next peer, linked list.
	struct _tracker_side_peer_t *next;
} tracker_peer_t;


// --------------------------------------------------------------- 
// Function prototpyes
// ---------------------------------------------------------------

// Thread to receive messages from a specific peer and respond if needed, by using peer-tracker
// handshake protocol 
void* handShakeMonitor(void* arg);

int dropPeerFromPeerTable(int sockfd);

int compareFileTables(filetable_t* sent_peer_file_table, struct in_addr peer_ip, int mode);

int broadcastFileUpdateToPeers();

void printTrackerPeerTable(tracker_peer_t* tracker_peer_table);


// Thread to check the alive status from online peers periodically.
void* monitorAlive(void* arg);

// ------------------ Nonthread functions ------------------ 

// Flushes out more information of the peer based on a REGISTER message caught from a connection.
void addInfoToPeerTableEntry(tracker_peer_t* tableHead, struct in_addr* ip, unsigned long last_time_stamp, int sockfd);

#endif