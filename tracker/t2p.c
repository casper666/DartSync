/*
- t2p.c (tracker to peer AND peer to tracker)

This header file should contain all structures and functions for maintaining tracker to peer communication.

Specifically, the functions for encapuslating file information should be in this .h and .c.

*/

// ---------------------------------------------------------------
// Header files
// ---------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include "t2p.h"
#include "../common/common.h" 


int TTPSendFileT(int peer_conn, ptp_tracker_t *data) {
	//printf("type:%d\n",data->type);
	int length = send(peer_conn, data, sizeof(ptp_tracker_t), 0);
	if ( length < 0){
		printf("TTP file send error\n");
		return -1;
	}
	printf("send %d success!!!\n",length);
	return 1;
}

int PTTRecvFileT(int peer_conn, ptp_peer_t *data) {
	//ptp_peer_t data_in;
	memset(data,0,sizeof(ptp_peer_t));
	int length = sizeof(ptp_peer_t);
	int cur = 0;
	while (cur < length){
		int size = cur + 1024 <= length ? 1024 : length - cur;
		int result = recv(peer_conn, ((char*)data)+cur, size, 0);
		if(result < 0) return -1;
		//printf("result of recv function is : %d, cur position : %d, sizeof(ptp_peer_t) is %lu\n", result, cur, sizeof(ptp_peer_t));
		//memcpy(data, (char*)&data_in+cur, 128);
		cur += result;
	}
	printf("recv function from %d return!!!!!\n",peer_conn);
	return 1;
}

//convert fileTable struct to char array
char* tableToCharArray(filetable_t* fileTable) {
	// ptp_tracker_t* ptp_tracker = (ptp_tracker_t*)malloc(sizeof(ptp_tracker_t));
	// memset(ptp_tracker,0,sizeof(ptp_tracker_t));
	char* tableArray = malloc(sizeof(filetable_t) + (fileTable->filenumber) * sizeof(filetable_entry_t));
	//printf("return array size %lu\n",sizeof(filetable_t) + (fileTable->filenumber) * sizeof(filetable_entry_t));
	memset(tableArray,0,sizeof(filetable_t) + (fileTable->filenumber) * sizeof(filetable_entry_t));
	memcpy(tableArray,fileTable,sizeof(filetable_t));
	int position = sizeof(filetable_t);
	filetable_entry_t* cur = fileTable->head;
	while(cur != NULL){
		//printf("cur position is %d\n",position);
		memcpy(tableArray+position,cur,sizeof(filetable_entry_t));
		((filetable_entry_t*)&tableArray[position])->next = (filetable_entry_t*)&tableArray[position + sizeof(filetable_entry_t)];
		cur = cur->next;
		position = position + sizeof(filetable_entry_t);
	}
	return tableArray;
}

//convert char array to fileTable struct
filetable_t* charArrayTotable(char* tableArray, int fileNum) {
	filetable_t* fileTable = (filetable_t*)malloc(sizeof(filetable_t));
	memset(fileTable,0,sizeof(filetable_t));
	memcpy(fileTable,tableArray,sizeof(filetable_t));
	fileTable->head = NULL;
	fileTable->tail = NULL;
	int position = sizeof(filetable_t);
	//filetable_entry_t* cur = (filetable_entry_t*)&tableArray[position];
	for(int i = 0; i < fileNum; i++){
		//printf("cur position is %d\n",position);
		//initialize filetable_entry
		filetable_entry_t* filetable_entry = (filetable_entry_t*)malloc(sizeof(filetable_entry_t));
		memset(filetable_entry,0,sizeof(filetable_entry_t));
		//copy filetabl_entry
		memcpy(filetable_entry,&tableArray[position],sizeof(filetable_entry_t));
		position = position + sizeof(filetable_entry_t);
		//append to tail
		if(fileTable->tail == NULL){
			fileTable->head = filetable_entry;
			fileTable->tail = filetable_entry;
		}else{
			fileTable->tail->next = filetable_entry;
			fileTable->tail = filetable_entry;
		}
	}
	if(fileTable->tail != NULL) fileTable->tail->next = NULL;
	return fileTable;
}
