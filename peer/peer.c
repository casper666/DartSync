/*
* Created: Jamie Yang 5/20/15

*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "peer.h"
//#include "../common/common.h"
//#include "../common/constants.h"
#include "ptp.h"
#include "p2t.h"
#include "../filemonitor/filetable.h"

//global variables
peer_peer_t *peer_list_t;	// list of peers
struct in_addr *my_addr, tracker_addr;
int trackerfd, ALIVE_SLEEP, PIECE_LEN;
//pthread_t peer_listen_thread[MAX_PEER_THREADS];
filetable_t* fileTable;
int monitorFlag;

char *download_buf[MAX_CONCUR_DOWNLOADS];	// maximum concurrent downloads for each peer

// initalizes peer_list_t
int PeerDownloadTableInit(peer_peer_t peer_list_t)
{
	return 1;
}

int PTPCheckDLEntry(peer_peer_t table, char *file_name, int sockfd)
{
	return 0;
}

int PTPCreateDownloadEntry(peer_peer_t *table, peer_peer_t *node)
{
	// create download request in table
	// not sure if this function is necessary
	return 0;
}

void* alive(void* arg){

	ptp_peer_t *alive = malloc(sizeof(ptp_peer_t));
	memset(alive,0,sizeof(ptp_peer_t));
	alive->type = KEEP_ALIVE;
	alive->peer_ip= *my_addr;
	alive->port = LISTENING_PORT;

	while(1){
		/*send alive message*/
		if(PTTSendFileT(trackerfd, alive) < 0){
			fprintf(stdout, "Failed to send alive message \n");
			pthread_exit(NULL);
		}

		/*sleep*/
		sleep(ALIVE_SLEEP);

	}

	free(alive);

}

int getIPFromHostname(struct in_addr * dest, char* hostname){

    //struct addrinfo *result;
    //struct addrinfo hints;
	struct hostent *hostInfo;
    struct in_addr addr;
    printf("[getIPbyID]hostname is %s\n",hostname);
    hostInfo = gethostbyname(hostname);
    if(!hostInfo) {
    	printf("host name error!\n");
        return -1;
    }
    memcpy(&addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
    memcpy(dest,&addr,sizeof(struct in_addr));
    return 1;
}

// waits for TCP connection to tracker (or are we connecting to tracker?)
int PTTConnOpen(){

	/*connect to the tracker*/
	fprintf(stdout, "Starting to connect to tracker\n");

	/*Create Peer socket to connect to tracker*/
	int tracker_sockfd;
	struct sockaddr_in saddr; 

	int iSetOption = 1;
	tracker_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	setsockopt(tracker_sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption, sizeof(iSetOption));

	if(tracker_sockfd < 0){
		fprintf(stderr, "Error: Could not create socket.\n");
		return -1;
	}

	fprintf(stdout, "Created peer socket to connect to tracker \n");
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(HANDSHAKE_PORT);
	saddr.sin_addr = tracker_addr;

	if(connect(tracker_sockfd, (struct sockaddr *) &saddr, sizeof(struct sockaddr_in))<0){
		fprintf(stderr, "Error: Failed to connect.\n");
		close(tracker_sockfd);
		return -1;
	}
	fprintf(stdout, "Connected to tracker \n");
	return tracker_sockfd;
}

int PTTConnClose(int conn){
	close(conn);
	return 1;
}

void peerStop(){
	free(my_addr);
	//free(tracker_addr);
	exit(EXIT_SUCCESS);

}

//filemonitor thread
void *fileMonitor(void* arg){
	char* dir = (char *)arg;
	//start monitor
	//startMonitor(argv[1],fileTable);
	printf("file monitor thread starts!!\n");
	while(monitorFlag){
		sleep(10);
		printf("scan.......\n");
		filetable_t* temp_fileTable = create_filetable();
		fileinfo_entry_t* head = getAllFilesInfo(dir,0);
		//printFileInfo(head);
		int err = init_filetable(temp_fileTable, head, dir);
		//printFileTable(temp_fileTable);
		if(err < 0) printf("initialize filetable fail\n");
		// printf("===============\n");
		// printf("update info:\n");
		int isSame = update_filetable(fileTable,temp_fileTable,UPDATE);
		// printf("===============\n");
		if(isSame == 1){
			printFileTable(fileTable);
			//fill the packet
			ptp_peer_t* ptp = (ptp_peer_t*)malloc(sizeof(ptp_peer_t));
			memset(ptp,0,sizeof(ptp_peer_t));
			memcpy(&(ptp->peer_ip),my_addr,sizeof(struct in_addr));
			ptp->port = HANDSHAKE_PORT;
			ptp->type = FILE_UPDATE;
			ptp->file_table_size = fileTable->filenumber;
			memcpy(ptp->file_table,tableToCharArray(fileTable),sizeof(filetable_t) + (fileTable->filenumber) * sizeof(filetable_entry_t));
			//print sent filetable
			printFileTable(charArrayTotable(ptp->file_table,ptp->file_table_size));
			//send the filetable
			printf("find local file changes!!!\n");
			PTTSendFileT(trackerfd, ptp);
			printf("send to tracker latest filetable on %d!!\n",trackerfd);
			printf("size of filetable_t: %lu\n",sizeof(filetable_t));
		}
		destroy_filetable(temp_fileTable);
	}
	pthread_exit(NULL);
}

int main(int argc, char **argv){
	/*get hostname as input from user */
	char hostname[MAX_HOSTNAME];
	memset(hostname, 0, MAX_HOSTNAME);
    	printf("Enter server name to connect:");
    	scanf("%s",hostname);
	
	if(getIPFromHostname(&tracker_addr, hostname)<0){
		fprintf(stdout, "Couldn't get server IP \n");
		exit(EXIT_FAILURE);
	}

	printf("get server ip success\n");

	/*Connect to tracker on given server*/
	if((trackerfd = PTTConnOpen())<0){
		fprintf(stdout, "Failed to open connection on %d to tracker \n",trackerfd);
		exit(EXIT_FAILURE);
	}

    /*register a signal handler which is used to terminate the process*/
    signal(SIGINT, peerStop);

	/*get myIP and store in global variable*/

    char myhostname[MAX_HOSTNAME];
    memset(myhostname, 0, MAX_HOSTNAME);
    gethostname(myhostname, sizeof(myhostname));
   	my_addr = calloc(1, sizeof(struct in_addr));

	//if(getIPFromHostname(my_addr, myhostname)<0){
   	if(getIPFromHostname(my_addr, myhostname) < 0){
		fprintf(stdout, "Failed to get my IP from hostname \n");
		exit(EXIT_FAILURE);
	}

	/*initialize local filetable*/
	fileTable = create_filetable();
	fileinfo_entry_t* head = getAllFilesInfo(argv[1],0);
	//printFileInfo(head);
	monitorFlag = 1;
	//create the file table
	fileTable = create_filetable();
	//initialize the file table with current whole directory information
	int err = init_filetable(fileTable, head, argv[1]);
	if(err < 0) printf("initialize filetable fail\n");
	//print the filetable
	printf("initial local filetable:\n");
	printFileTable(fileTable);

    /*Get timer ready*/
    struct timespec tim;
    tim.tv_sec=0;
    tim.tv_nsec= HANDSHAKE_WAIT;
    nanosleep(&tim, NULL);


	/*Create and send register message*/
	ptp_peer_t* reg = (ptp_peer_t*)malloc(sizeof(ptp_peer_t));
	memset(reg,0,sizeof(ptp_peer_t));
	reg->type = REGISTER;
    reg->peer_ip = *my_addr;
    reg->port = LISTENING_PORT;
    reg->file_table_size = 0;
    if(PTTSendFileT(trackerfd, reg)<0){
        fprintf(stdout, "Failed to send register message \n");
        //pthread_exit(NULL);
    }
    free(reg);
	
	/*Receive accept message that contains new info*/
	ptp_tracker_t* accpt = (ptp_tracker_t*)malloc(sizeof(ptp_tracker_t));;
	memset(accpt,0,sizeof(ptp_tracker_t));
	if(TTPRecvFileT(trackerfd, accpt)<0){
		fprintf(stdout, "Failed to receive accept message from server \n");
		exit(EXIT_FAILURE);

	}
	ALIVE_SLEEP = accpt->interval;
	printf("interval time is %d\n",ALIVE_SLEEP);
	PIECE_LEN = accpt->piece_len;
	printf("piece_len is %d\n",PIECE_LEN);
	printf("tracker side file num: %d\n",accpt->file_table_size);

	//get copy of local filetable
	char* buffer = malloc(MAX_TABLE_LEN);
    memset(buffer,0,MAX_TABLE_LEN);
    memcpy(buffer,tableToCharArray(fileTable),sizeof(filetable_t) + (fileTable->filenumber) * sizeof(filetable_entry_t));
    
	free(accpt);

	/*send local filetable to server*/
	//fill the packet
	ptp_peer_t* ptp = (ptp_peer_t*)malloc(sizeof(ptp_peer_t));
	memset(ptp,0,sizeof(ptp_peer_t));
	memcpy(&(ptp->peer_ip),my_addr,sizeof(struct in_addr));
	ptp->port = HANDSHAKE_PORT;
	ptp->type = FILE_UPDATE;
	ptp->file_table_size = fileTable->filenumber;
	memcpy(ptp->file_table,tableToCharArray(fileTable),sizeof(filetable_t) + (fileTable->filenumber) * sizeof(filetable_entry_t));
	//send the filetable
	printf("initialization process....\nsend local filetable to tracker\n");
	PTTSendFileT(trackerfd, ptp);
	printf("send to tracker latest filetable on %d!!\n",trackerfd);

	/*Create alive thread*/
	pthread_t alive_thread;
	pthread_create(&alive_thread,NULL,alive,(void*)0);
	printf("alive thread start\n");

	// create listening thread
	pthread_t listening_thread;
	pthread_create(&listening_thread, NULL, PTPListen, NULL);

	/*Start the file monitor thread*/
	pthread_t monitor_thread;
	pthread_create(&monitor_thread, NULL, fileMonitor, (void*)argv[1]);


	/*create download threads if we receive a broadcast message from tracker*/
	ptp_tracker_t* broadcast = malloc(sizeof(ptp_tracker_t));
	while(TTPRecvFileT(trackerfd, broadcast)>0 ){

		//if there's a change in the file table from an IP that's not our own
		filetable_t *file_table = charArrayTotable(broadcast->file_table, broadcast->file_table_size);

		printf("receve broadcast filetable:\n");
		printFileTable(file_table);

		blockMonitor(fileTable);
		printf("lock monitor\n");
		//sees if filetable has changed and starts download thread if necessary
		if(compare_filetable(fileTable, file_table) < 0){
			fprintf(stderr, "File Table is the same, local files are all up-date.\n");
		}
		free(file_table);
		unblockMonitor(fileTable);
		printf("unlock monitor\n");

	}
	return 0;
}
