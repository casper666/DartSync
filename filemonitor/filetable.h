//FILE: filemonitor/filetable.h
//
//Description: this file defines the data structures and functions used by fileMonitor process
//
//Date: May 19,2015

#ifndef FILETABLE_H
#define FILETABLE_H

#include <pthread.h>
#include "../common/common.h"
#include "../common/constants.h"

//create the filetable and malloc all necessary memory
filetable_t* create_filetable();

//initialize the filetable, fill necessary parts
int init_filetable(filetable_t* fileTable, fileinfo_entry_t* fileInfo, char* directory);

//add a file_entry_t to a filetable
int add_filetable_entry(filetable_t* fileTable, filetable_entry_t* fileInfo);

//delete a file_entry_t to from filetable
int delete_filetable_entry(filetable_t* fileTable, filetable_entry_t* fileInfo);

//modify a file_entry_t in a filetable
int modify_filetable_entry(filetable_t* fileTable, filetable_entry_t* fileInfo);

//compare two filetables - called by peer main
int compare_filetable(filetable_t* fileTable, filetable_t* temp_fileTable);

//update a filetable
int update_filetable(filetable_t* fileTable, filetable_t* temp_fileTable, int mode);

//5/26: updates filetable and adds peer information where needed, accordingly.
int update_filetable_and_peerLists (filetable_t* fileTable, filetable_t* temp_fileTable, int mode, struct in_addr peer_ip, int peer_portNum);

//destroy a filetable, free all necessary memory
int destroy_filetable(filetable_t* filetable);

//get all the file info given a path
fileinfo_entry_t* getAllFilesInfo(const char* path, int level);

//get the file info given a filename
fileinfo_entry_t* getFileInfo(const char* filename, int type);

//print fileinfo linked list
void printFileInfo(fileinfo_entry_t* entry);

//print filetable
void printFileTable(filetable_t* fileTable);

//remove a dead peer from the filetable
void removePeerFromTable(struct in_addr addr, filetable_t* fileTable);

//remove one dead peer for a file's peerlist
int removePeerFromList(struct in_addr addr, peer_entry_t* peerList, filetable_entry_t* fileEntry);

//removes all peers for a file's peerlist
int removeAllPeersFromList(peer_entry_t* peerList, filetable_entry_t* fileEntry);

//remove all peers for a file's peerlist in a filetable
int removeAllPeersFromTable(filetable_t* fileTable, char* filename);

//add one peer to a file's peerlist
int addPeerToList(struct in_addr addr, int portNum, peer_entry_t* peerList, filetable_entry_t* fileEntry);

//add one peer to a filetable
int addPeerToTable(struct in_addr addr, int portNum, filetable_t* fileTable, char* filename);

//block the monitor
void blockMonitor(filetable_t* fileTable);

//unblock the monitor
void unblockMonitor(filetable_t* fileTable);

//begin download
int initDownload(filetable_entry_t* file_entry);

#endif
