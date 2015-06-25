/*
	- common.h 

	This file should be used for common structure definitions. E.g. The filetable. 

*/

#ifndef COMMON_H
#define COMMON_H


#include <netinet/in.h>
#include <pthread.h>
#include "./constants.h"

// ---------------------------------------------------------------
// Structure definitions
// ---------------------------------------------------------------

// --- The Filetable Structures (Boying 5/20: File table structure is the same on
// the peer and tracker but will not necessarily have the same content at a 
// given point in time.) --- 


//peer struct used in peerlist
typedef struct peer{
	struct in_addr addr;
	int portNum;
	int sockfd;
} peer_entry_t;

//fileinfo_entry_t is the fileinfo entry contained in the fileinfo table.
typedef struct fileinfo_entry {
	char filename[FILE_NAME_LEN]; //filename
	unsigned long size; //size of the file
	unsigned long int timestamp; //timestamp of the file
	int type;	//type of the file
	struct fileinfo_entry* next;	//next entry pointer
} fileinfo_entry_t;

//filetable_entry_t is the filetable entry contained in the file table.
typedef struct filetable_entry {
	fileinfo_entry_t fileInfo; //file info of this file
	int peerNum; //total peer numbers
	peer_entry_t peerList[MAX_PEER_NUMBER]; //peer list for this file
	struct filetable_entry* next; //next entry pointer
	int isSync; //Sync Flag, currently unused
} filetable_entry_t;

//filetable is a linkedlist of filetable_entry_t, also it contains a mutex.
typedef struct filetable{
	int filenumber; //file number in this directory
	//pthread_mutex_t mutex; //mutex for this file table
	int isBlocked; //flag for blocking monitor
	filetable_entry_t* head; //header pointer
	filetable_entry_t* tail; //tail pointer
} filetable_t;

/*Packet Struct to send from tracker to peer*/
typedef struct segment_tracker{
        // time interval that the peer should sending alive message periodically
        int interval;
        // piece length
        int piece_len;
        // file number in the file table -- optional
        int file_table_size;
        // file table of the tracker -- your own design
        // 5/24 fho: acts as a buffer, when passed from T2P, it should hold a linked list of the filetable entries ("filetable_entry_t"s back to back).        
        char file_table[MAX_TABLE_LEN];
} ptp_tracker_t;


/*Packet struct to send from peer to tracker*/
typedef struct segment_peer {
	// protocol length
	int protocol_len;
	// protocol name
	char protocol_name[PROTOCOL_LEN + 1];
	// packet type : register, keep alive, update file table
	int type;
	// reserved space, you could use this space for your convenient, 8 bytes by default
	char reserved[RESERVED_LEN];
	// the peer ip address sending this packet
	struct in_addr peer_ip;
	// listening port number in p2p
	int port;
	// the number of files in the local file table -- optional
	int file_table_size;
	// file table of the client -- your own design
	// 5/24 fho: acts as a buffer, when passed from P2T, it should hold a linked list of the filetable entries ("filetable_entry_t"s back to back).        
	char file_table[MAX_TABLE_LEN];
}ptp_peer_t;

#endif

