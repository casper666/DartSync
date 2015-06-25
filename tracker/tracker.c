/*
	tracker.c
	Last author: fho, 5/20
	Main thread, handShakeMonitor(), and aliveMonitor()
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include "tracker.h"

// Globals
	// For when either the handShakeMonitor() or monitorAlive() threads make adjustments to the tracker's peer table.
	pthread_mutex_t* tracker_peer_t_mutex;
	// For when either the handShakeMonitor() or monitorAlive() threads make adjustments to the tracker's file table.
	pthread_mutex_t* tracker_file_t_mutex;
	// Tracker's file table.
	filetable_t* tracker_file_table;
	// Tracker's peer table.
	tracker_peer_t* tracker_peer_table;
	// Check whether the monitorAlive() thread has started.
	int startedMonitorThread = 0;
//

//print filetable
void printTrackerPeerTable(tracker_peer_t* tracker_peer_table){
	tracker_peer_t* cur = tracker_peer_table;
	printf("=======print peer table========\n");
	while(cur != NULL){
		printf("peer addr: %s TCP conn %d\n",inet_ntoa(cur->ip),cur->sockfd);
		cur = cur->next;
	}
	printf("===============================\n");
	return;
}

// Note: should not get into here without at least one peer. 
void addInfoToPeerTableEntry(tracker_peer_t* tableHead, struct in_addr* ip, unsigned long last_time_stamp, int sockfd){
	tracker_peer_t* nextPeer = tableHead;
	while (nextPeer->next) {
		if (nextPeer->sockfd == sockfd) {
			if (nextPeer->ip.s_addr != ip->s_addr) {
				memcpy(&nextPeer->ip,ip,sizeof(struct in_addr));
				nextPeer->thread_id = pthread_self();
			}
			nextPeer->last_time_stamp = last_time_stamp;
			return;
		}
		nextPeer = nextPeer->next;
	}
	if (nextPeer->sockfd == sockfd) {
		if (nextPeer->ip.s_addr != ip->s_addr) {
			memcpy(&nextPeer->ip,ip,sizeof(struct in_addr));
			nextPeer->thread_id = pthread_self();
		}	
		nextPeer->last_time_stamp = last_time_stamp;
		return;
	}	
	return;
}

int dropPeerFromPeerTable(int sockfd) {
	tracker_peer_t* nextPeer = tracker_peer_table;
	tracker_peer_t* holdingPreviousPeer = nextPeer;
	while (nextPeer->next) {
		if (nextPeer->sockfd == sockfd) {
			tracker_peer_t* thisPeer = nextPeer;
			holdingPreviousPeer->next = nextPeer->next;
			free(thisPeer);
			return 1;
		}
		nextPeer = nextPeer->next;
		holdingPreviousPeer = nextPeer;
	}
	if (nextPeer->sockfd == sockfd) {
		free(nextPeer);
		return 1;
	}	
	return -1;
}

int clearFileTable(filetable_entry_t* fileTableEntryHead) {
	int freedSomething = 0;
	while (fileTableEntryHead) {
		freedSomething = 1;
		free(fileTableEntryHead);
		if (fileTableEntryHead->next) {
			fileTableEntryHead = fileTableEntryHead->next;
		}
		else 
			break;
	}
	if (freedSomething == 1) {
		return 1;
	}
	else {
		return -1;
	}
}

/*
	Return -1 nothing is updated, return 1 if something is updated.
*/

// 5/23: STILL NEED TO IMPLEMENT THIS
int compareFileTables(filetable_t* sent_peer_file_table, struct in_addr peer_ip, int mode) {
	int aTrackerFileTableUpdated = 1;
	printf("begin update\n");
	aTrackerFileTableUpdated = update_filetable_and_peerLists(tracker_file_table, sent_peer_file_table, mode, peer_ip, HANDSHAKE_PORT);
	if (aTrackerFileTableUpdated == -1) {
		printf("[compare]filetable same\n");
		return -1;
	}
	// Both files are the same.
	else {
		printf("[compare]filetable changed\n");
		return 1;
	}
}

/*
	take the latest file table, send it out to all the peers.
*/
int broadcastFileUpdateToPeers() {
	tracker_peer_t* nextPeer = tracker_peer_table;
	while (nextPeer) {
		ptp_tracker_t* broadcast = calloc(1,sizeof(ptp_tracker_t));
		broadcast->file_table_size = tracker_file_table->filenumber;
		memcpy(broadcast->file_table,tableToCharArray(tracker_file_table), sizeof(filetable_t) + (tracker_file_table->filenumber) * sizeof(filetable_entry_t));
		TTPSendFileT(nextPeer->sockfd, broadcast);
		printf("----> send broadcast to peer %s\n",inet_ntoa(nextPeer->ip));
		printFileTable(charArrayTotable(broadcast->file_table,broadcast->file_table_size));
		free(broadcast);
		nextPeer = nextPeer->next;
	}
	printf("broadcast finish.\n");
	return 0;
}


void* handShakeMonitor(void* arg) {
	printf("- tracker.c:handShakeMonitor() thread started..\n");
	int peer_conn = *(int*) arg;
	free((int*)arg);
	// ---------- Pseudo for handShakeMonitor() -----------
	// On a successful packet capture (Peer --> Tracker) , check the type of PACKET
	ptp_peer_t* newPeerPkg = (ptp_peer_t*) calloc(1,sizeof(ptp_peer_t));
	// Use stat struct to grab the time.
	//struct stat statbuff;
	while (1) {
		time_t cur = time(NULL);
		if (PTTRecvFileT(peer_conn, newPeerPkg) < 0) {
			printf("tracker.c:handShakeMonitor(): error with tracker catching peer package.\n");
			//exit(1);
			printf("now quit handshake thread for conn %d\n",peer_conn);
			pthread_exit(NULL);
		}
		else {
			// switch([type of packet])
			int type = newPeerPkg->type;
			printf("receive type %d from %d\n",type,peer_conn);
			switch (type) {
				// REGISTER: (other fields should be null)
				case REGISTER:
					// Flush out more information about this peer
					printf("get register message from connection: %d\n",peer_conn);
					//continue;
					//struct stat statbuff;
					pthread_mutex_lock(tracker_peer_t_mutex);
			    	//memset(&statbuff,0,sizeof(statbuff));
			    	//time_t cur = time(NULL);
					//addInfoToPeerTableEntry(tracker_peer_table, newPeerPkg->peer_ip, (unsigned long) statbuff.st_mtime, *peer_conn);
					addInfoToPeerTableEntry(tracker_peer_table, &newPeerPkg->peer_ip, (long) cur, peer_conn);
					pthread_mutex_unlock(tracker_peer_t_mutex);
					//print the peer table
					printTrackerPeerTable(tracker_peer_table);
					// send back a packet informing the INTERVAL and PIECE_LENGTH for the peer to set up
					ptp_tracker_t *startPeer = calloc(1,sizeof(ptp_tracker_t));
					// ******INTERVAL == ALIVE_TIME/4 (2.5 minutes) (Want this to be shorter than the alive time so that we guarentee this peer tracker connection doesn't die)
					startPeer->interval = ALIVE_TIME/4;
					startPeer->piece_len = PIECE_LENGTH;

					int isInitialized = 0;
					
					// Send out to peer conn.
					TTPSendFileT(peer_conn, startPeer);
					free(startPeer);
					break;
				// KEEP ALIVE:
				case KEEP_ALIVE:
					printf("get keep_alive message from connection: %d\n",peer_conn);
					// this peer is alive; don't drop in the next monitor alive cycle. Do this by updating the last alive time stamp.
			    	pthread_mutex_lock(tracker_peer_t_mutex);
			    	//memset(&statbuff,0,sizeof(statbuff));
			    	//time_t cur = time(NULL);
					addInfoToPeerTableEntry(tracker_peer_table, &newPeerPkg->peer_ip, (long) cur, peer_conn);	    	
					pthread_mutex_unlock(tracker_peer_t_mutex);
					break;
				// FILE UPDATE: 
				case FILE_UPDATE:
					printf("get file_update message from connection: %d\n",peer_conn);
					pthread_mutex_lock(tracker_file_t_mutex);
					printf("\ttracker:handShakeMonitor(): FILE_UPDATE packet caught from peer\n");
					filetable_t* peer_sent_file_table = charArrayTotable(newPeerPkg->file_table, newPeerPkg->file_table_size);
					printf("receive filetable from peer:\n");
					printFileTable(peer_sent_file_table);
					if(!isInitialized){
						printf("\tPeer filetable was not intialized.\n");
						if (compareFileTables(peer_sent_file_table, newPeerPkg->peer_ip, INIT) > 0) {
							printf("\t\tthis is the initialization process\n");
							printf("\t\tfiletable changed, need broadcast\n");
							printFileTable(tracker_file_table);
							broadcastFileUpdateToPeers();
						}
						else {
							// update not necessary. but for initialization it should broadcast
							printf("\t\tthis is the initialization process\n");
							printf("\t\tfiletable remains the same, don't need broadcast, broadcast for pause!!\n");
							broadcastFileUpdateToPeers();
						}
						isInitialized = 1;
					} 
					else{
						printf("\tPeer filetable was intialized.\n");
						if (compareFileTables(peer_sent_file_table, newPeerPkg->peer_ip, UPDATE) > 0) {
							printf("\t\tfiletable changed, need broadcast\n");
							printFileTable(tracker_file_table);
							broadcastFileUpdateToPeers();
						}
						else {
							// update not necessary.
							printf("\t\tfiletable remains the same, don't need broadcast\n");
							printFileTable(tracker_file_table);
						}
					}
					pthread_mutex_unlock(tracker_file_t_mutex);
					break;
			}
		}
	}
	free(newPeerPkg);
	pthread_exit(NULL);
}

void* monitorAlive(void* arg) {
	printf("enter monitorAlive thread\n");
	//pthread_t monitorAliveId = pthread_self();
	while (1) {
		sleep(ALIVE_TIME);
		printf("monitorAlive checks..\n");
		int peerNum = 0;
		pthread_mutex_lock(tracker_peer_t_mutex);
		time_t current_time_stamp = time(NULL);
		tracker_peer_t* cur = tracker_peer_table;
		tracker_peer_t* pre = NULL;
		tracker_peer_t* next = NULL;
		while(cur != NULL){
			cur = cur->next;
			peerNum++;
		}
		cur = tracker_peer_table;
		while (cur->next) {
			next = cur->next;
			if (current_time_stamp - cur->last_time_stamp > ALIVE_TIME) {
				peerNum--;
				// TIME DIFFERENCE IS GREATER THAN ALIVE_TIME, DROP PEER FROM TABLE, KILL IT'S HANDSHAKE THREAD.
				printf("check if handshake thread exits\n");
				pthread_join(cur->thread_id,NULL);
				close(cur->sockfd);
				removePeerFromTable(cur->ip, tracker_file_table);
				printf("remove peer %s from file table\n",inet_ntoa(cur->ip));
				if(pre == NULL) tracker_peer_table = next;
				else pre->next = next;
				printf("remove peer %s from peer table\n",inet_ntoa(cur->ip));
				printFileTable(tracker_file_table);
				printTrackerPeerTable(tracker_peer_table);
				free(cur);
				pre = NULL;
				cur = next;
			}else{
				pre = cur;
				cur = next;
			}
		}
		if (cur) {
			next = cur->next;
			if (current_time_stamp - cur->last_time_stamp > ALIVE_TIME) {
				peerNum--;
				printf("check if handshake thread exits\n");
				pthread_join(cur->thread_id,NULL);
				close(cur->sockfd);
				startedMonitorThread = 0;
				removePeerFromTable(cur->ip, tracker_file_table);
				printf("remove peer %s from file table\n",inet_ntoa(cur->ip));
				if(pre == NULL) tracker_peer_table = next;
				else pre->next = next;
				printf("remove peer %s from peer table\n",inet_ntoa(cur->ip));
				free(cur);
				printFileTable(tracker_file_table);
				printTrackerPeerTable(tracker_peer_table);
				if(!peerNum){
					printf("no more peer connected, remove monitorAlive thread\n");
					pthread_exit(NULL);
				}
			}
		}
		pthread_mutex_unlock(tracker_peer_t_mutex);
	}
	pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
	printf("========================================================================\n");
	printf("============================TRACKER STARTING============================\n");
	printf("========================================================================\n");	
	// ---------- Pseudo for main -----------
	// Initalize tracker's peer table.
	tracker_peer_table = NULL;
	// Intialize tracker's file table.
	tracker_file_table = create_filetable();
	// Initialize mutexes
	tracker_peer_t_mutex = calloc(1,sizeof(pthread_mutex_t));
	pthread_mutex_init(tracker_peer_t_mutex, NULL);
	tracker_file_t_mutex = calloc(1,sizeof(pthread_mutex_t));
	pthread_mutex_init(tracker_file_t_mutex, NULL);
	// TCP connection stuff -----------------------------------------------------------
	int tcpserv_sd;
	struct sockaddr_in tcpserv_addr;
	//int peer_t_connection;
	struct sockaddr_in tcpclient_addr;
	socklen_t tcpclient_addr_len;
	tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0); 
	// ========================= PREVENT OVERLAY FAILURE ==============================	
	int yes=1;
	if (setsockopt(tcpserv_sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
	    perror("setsockopt");
	    exit(1);
	}		
	// ========================= PREVENT OVERLAY FAILURE ==============================	
	if(tcpserv_sd<0) 
		return -1;
	memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
	tcpserv_addr.sin_family = AF_INET;
	tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpserv_addr.sin_port = htons(HANDSHAKE_PORT);
	if(bind(tcpserv_sd, (struct sockaddr *)&tcpserv_addr, sizeof(tcpserv_addr))< 0)
		return -1; 
	if(listen(tcpserv_sd, MAX_PEER_NUMBER) < 0) 
		return -1;
	printf("tracker:main(): waiting for a peer connection...\n");
	// TCP connection stuff -----------------------------------------------------------

	// Infinite loop listening for peers to connect.
	while (1) {
		// Wait on HANDSHAKE PORT to capture a connection from a peer.
		int* peer_t_connection = malloc(sizeof(int));
		*peer_t_connection = accept(tcpserv_sd,(struct sockaddr*)&tcpclient_addr,&tcpclient_addr_len);
		// If connection is established, 

		// IMPORTANT: (NEED A DELAY ON THE PEER SIDE AFTER A SUCCESSFUL CONNECTION BEFORE THE PEER SENDS A REGISTER PACKET)
		if (*peer_t_connection < 0) {
			printf("Faulty peer-tracker connection!\n");
		}
		else {
			printf("\ttracker:main(): new peer connection established on: %d!\n", *peer_t_connection);			
			// add the peer to peer table.
			pthread_mutex_lock(tracker_peer_t_mutex);
			if (!tracker_peer_table) {
				// First peer in the table.
				tracker_peer_table = (tracker_peer_t*) calloc(1,sizeof(tracker_peer_t));
				tracker_peer_table->sockfd = *peer_t_connection;
			}
			else {
				// append to the tail
				tracker_peer_t* cur = tracker_peer_table;
				while(cur->next != NULL){
					cur = cur->next;
				}
				if(cur != NULL){
					tracker_peer_t* newPeer = (tracker_peer_t*) calloc(1,sizeof(tracker_peer_t));
					newPeer->sockfd = *peer_t_connection;
					cur->next = newPeer;
				}		
			}
			printTrackerPeerTable(tracker_peer_table);
			pthread_mutex_unlock(tracker_peer_t_mutex);
			printf("add connection %d to peer_table\n",*peer_t_connection);
			// ^Add more information to this peer entry as the first REGISTER packet is caught.

			// spawn a new handShakeMonitor() thread.
			pthread_t handShakeMonitorThread;
			pthread_create(&handShakeMonitorThread,NULL,&handShakeMonitor,(void*)peer_t_connection);
			pthread_detach(handShakeMonitorThread);
			printf("\ttracker:main(): starting a new handShakeMonitorThread for peer connection %d\n", *peer_t_connection);
			// Additionally, create a single monitorAlive() thread. 
			if (startedMonitorThread == 0) {
				printf("\t Starting monitorAliveThread... \n");
				pthread_t monitorAliveThread;
				pthread_create(&monitorAliveThread,NULL,&monitorAlive,0);
				pthread_detach(monitorAliveThread);
				startedMonitorThread = 1;
			}
		}	
	}
	return 0;
}
