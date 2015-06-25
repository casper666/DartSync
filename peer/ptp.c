 /*
* Modified: Jamie Yang 5/20/15
*/

// sends file partition to peer
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
#include <math.h>
#include "ptp.h"
#include "../common/common.h"
#include "../common/constants.h"
#include "./resume_dl.h"

int download_timeout = 1;
pthread_mutex_t write_lock;

// always on, listens for download requests so that we can open upload threads
void* PTPListen(void* arg)
{
	int tcpserv_sd;
	struct sockaddr_in tcpserv_addr;
	struct sockaddr_in tcpclient_addr;
		memset(&tcpclient_addr, 0, sizeof(tcpclient_addr));
	socklen_t tcpclient_addr_len;
		memset(&tcpclient_addr_len, 0, sizeof(tcpclient_addr_len));

	tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0);
	int yes=1;
	if (setsockopt(tcpserv_sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
	    perror("setsockopt");
	    exit(1);
	}
	if(tcpserv_sd<0) {
		printf("Server sock fail\n");
		return NULL;
	}
	memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
	tcpserv_addr.sin_family = AF_INET;
	tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpserv_addr.sin_port = htons(LISTENING_PORT);

	if(bind(tcpserv_sd, (struct sockaddr *)&tcpserv_addr, sizeof(tcpserv_addr))< 0){
		printf("Server bind fail\n");
		return NULL; 
	}
	if(listen(tcpserv_sd, 5) < 0) {
		printf("Server listen fail\n");
		return NULL;
	}

	// Should be a table that is set with corresponding thread ids for multiple connect/disconnects
	while(1){
		printf("waiting for peer connection for downloading\n");
		int *conn = malloc(sizeof(int));
		*conn = accept(tcpserv_sd,(struct sockaddr*)&tcpclient_addr,&tcpclient_addr_len);
		if(*conn < 0){	//! invalid write size here
			printf("Error establishing connection\n");
			return NULL;
		}
		printf("Peer node connected for downloading, %d!\n", *conn);

		// Create upload thread
		pthread_t peer_listen_thread;
		pthread_create(&peer_listen_thread, NULL, PTPUpload, conn);
		printf("Creating new peer_listen_thread\n");
	}
}

// waits for multiple connections from peer to peer
void* PTPUpload(void* arg)
{
	int *peer_conn_pt = (int*)arg;

	int downloading = 1;
	int peer_conn = *peer_conn_pt;
	free(peer_conn_pt);
	printf("upload peer conn: %d\n", peer_conn);

	while (downloading){
		// receives seq_number from downloading peer
		partition_t data_in;

		if (PTPRecvFile(peer_conn, &data_in) < 0){
			printf("Receive file error\n");
			exit(1);
		}
		printf("Received data request of length: %lld sequence number: %lld\n", data_in.header.length, data_in.header.seq_num);

		unsigned long long int length = data_in.header.length;
		unsigned long long int seq_num = data_in.header.seq_num;
		int data_type = data_in.header.type;

		if (data_type == DOWNLOADING){
			// read file_name from data?
			// look at local file directory and read in bytes?
			char *file_name = data_in.header.file_name;
			printf("file name is: %s\n", file_name);

			//! http://www-01.ibm.com/support/knowledgecenter/SSB23S_1.1.0.8/com.ibm.ztpf-ztpfdf.doc_put.08/gtpc2/cpp_send.html?cp=SSB23S_1.1.0.8%2F0-3-7-1-0-9-2
			//! update this so that the data gets sent sequentially. maximum size of 1 GB
			//! grabs sequence number from file_name and sends it through using PTPSendFile(int peer_conn, partition_t *data);
			//! use flag to specify large file (set limit to 1GB?)
			
			// break up data into chunks of size MAX_DATA_LEN
			unsigned long long int send_len = 0;	  	// 
			unsigned long long int seq_num_send = 0; 	//
			long long int remain_len = length;

			while (remain_len > 0 && downloading){
				if (RES_DEF)
					usleep(10000);
				partition_t data_out;						// create packet to send
				memset(&data_out, 0, sizeof(partition_t));	
				data_out.header.type = UPLOADING;			
				data_out.header.seq_num = seq_num_send;		// sequence number of data packets sent

				// if remaining length < MAX_DATA_LEN - 1, send it all. else, send chunks of MAX_DATA_LEN - 1 size
				if ( remain_len < MAX_DATA_LEN - 1){
					send_len = remain_len;
				}else{
					send_len = MAX_DATA_LEN - 1;
				}
				//printf("sending: %lld\n", send_len);

				data_out.header.length = send_len;

				// grab a chunk of the file. Will fast I/O be an issue?
				char *file_data = GetFileData(file_name, seq_num + seq_num_send, length);	// grab file locally
				//printf("Getting file data\n");
				if (file_data == NULL){
					printf("Error reading file! Closing uploading thread.\n");
					break;
				}

				memcpy(data_out.data , file_data, send_len);

				if (PTPSendFile(peer_conn, &data_out) < 0){
					printf("Upload Error!\n");
					downloading = 0;
				}

				seq_num_send += send_len;
				printf("Seq_num_send: %lld\n", seq_num_send + seq_num);
				remain_len -= send_len;
				printf("Remain length: %lld\n", remain_len);
				printf("Sending file data[%d] remaining length: %lld/%lld\n", peer_conn, remain_len, length);
				if (remain_len < 0){
					printf("Math error with remain length\n");
					exit(EXIT_FAILURE);
				}

				free(file_data);
			}
		}else if (data_type == FINISH){
			printf("Closing uploader!\n");
			downloading = 0;
		}else{
			printf("Upload Error!\n");
			downloading = 0;
		}
	}

	//PTPConnClose(peer_conn);
	pthread_exit(NULL);
}

// when a download thread is created, the download thread takes in an argument that contains the file to be downloaded and the file length
// downloader is in charge of requesting segments from the uploader
// each thread will request different segments. how do we centralize this process? send as arguments the segments needed?
void* PTPDownload(void* arg)
{
	// parse argument info and send message to uploader containing sequence number, file name, file length, peer, IP, etc.
	dl_param_t *dl_args = (dl_param_t *) arg;

	// send connection request to peer by looking up IP table
	unsigned long long int seq_num_thread = dl_args->seq_num;
	printf("download sequence number: %lld\n", seq_num_thread);
	unsigned long long int length_thread = dl_args->seg_len;
	printf("length of thread: %lld\n", length_thread);

	int peer_conn = dl_args->sockfd;	// comes from argument

	int downloading = 1;
	unsigned long long int recv_buf = 0;
	char *file_name = dl_args->file_name;
	FILE *fp = dl_args->fp;
	fseek(fp, seq_num_thread, SEEK_SET);

	partition_t data; // read information from argument
	memset(&data, 0 , sizeof(partition_t));
	data.header.length = length_thread;
	data.header.seq_num = seq_num_thread;
	data.header.type = DOWNLOADING;
	strcpy(data.header.file_name, file_name);

	printf("Sending data request to peer\n");
	//! http://www-01.ibm.com/support/knowledgecenter/SSB23S_1.1.0.8/com.ibm.ztpf-ztpfdf.doc_put.08/gtpc2/cpp_send.html?cp=SSB23S_1.1.0.8%2F0-3-7-1-0-9-2
	//! update this so that the data gets sent sequentially. maximum size of 1 GB
	//! grabs sequence number from file_name and sends it through using PTPSendFile(int peer_conn, partition_t *data);
	//! use flag to specify large file (set limit to 1GB?)
	if(PTPSendFile(peer_conn, &data) < 0){
		printf("PTPSendFile Error!\n");
	}

	while(downloading){
		partition_t data_out;
		memset(&data_out, 0, sizeof(partition_t) );

		if (PTPRecvFile(peer_conn, &data_out) < 0){
			printf("Receive file error\n");
			exit(1);
		}

		if (data_out.header.type == UPLOADING){
			printf("Received data from peer[%d]: %lld/%lld\n", peer_conn, seq_num_thread + data_out.header.seq_num , length_thread);

			// mutex this write operation
			pthread_mutex_lock(&write_lock);
			fseek(fp, seq_num_thread + data_out.header.seq_num , SEEK_SET);
			printf("seq num write: %lld\n", seq_num_thread + data_out.header.seq_num);
			fwrite(data_out.data , 1, data_out.header.length, fp);

			// logic here to add download resume
			pthread_mutex_unlock(&write_lock);

			recv_buf += data_out.header.length;

			if (recv_buf == length_thread){
				printf("received buffer equals length of segment requested\n");
				downloading = 0;
			}else if(recv_buf > length_thread){
				// debug
				printf("Buffer calculation error!\n");
				exit(EXIT_FAILURE);
			}
		}else{
			printf("Download thread fail\n");
			break;
		}
	}

	partition_t data_finish; // read information from argument
	memset(&data_finish, 0 , sizeof(partition_t));
	data_finish.header.type = FINISH;
	PTPSendFile(peer_conn, &data_finish);

	printf("Download thread complete, closing connection\n");
	PTPConnClose(peer_conn);
	pthread_exit(NULL);
}

char* GetFileData(char *file_name, unsigned long long int seq_num, unsigned long long int file_len)
{
	// Takes in file_name (including directory information) and reads data into a buffer and returns that
	FILE *fp;
	fp = fopen(file_name,"r");

	if (fp == NULL){
		printf("Invalid file_name (uploader)\n");
		return NULL;
	}

	fseek(fp, 0, SEEK_SET);			// set seek to beginnig of file
	fseek(fp, seq_num, SEEK_CUR);	// set seek to seq_num from curr pos

	char *buf_in = malloc(file_len + 1);
	fread(buf_in, 1, file_len, fp);
	buf_in[file_len] = '\0';

	fclose(fp);
	return buf_in;
}

int PTPConnOpen(char *hostname){
	struct sockaddr_in servaddr;
	struct hostent *hostInfo;

	hostInfo = gethostbyname(hostname);
	if(!hostInfo) {
		printf("host name error!\n");
		return -1;
	}
		
	servaddr.sin_family =hostInfo->h_addrtype;	
	memcpy((char *) &servaddr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);

	servaddr.sin_port = htons(LISTENING_PORT);

	int network_conn = socket(AF_INET,SOCK_STREAM,0);
	if(network_conn<0){
		printf("socket failure\n");
		return -1;
	}

	if(connect(network_conn, (struct sockaddr*)&servaddr, sizeof(servaddr))!=0) {
		printf("connection failure\n");
		return -1;
	}

	//succefully connected
	printf("Opening connection to: %d\n", network_conn);
	return network_conn;
}

int PTPConnOpenFile(struct in_addr addr){
	struct sockaddr_in servaddr;
	struct hostent *hostInfo;

	//uint32_t network_addr = addr.s_addr;
	char *ip = inet_ntoa(addr);

	hostInfo = gethostbyname(ip);
	if(!hostInfo) {
		printf("host name error!\n");
		return -1;
	}
		
	servaddr.sin_family = hostInfo->h_addrtype;	
	memcpy((char *) &servaddr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);

	servaddr.sin_port = htons(LISTENING_PORT);

	int network_conn = socket(AF_INET,SOCK_STREAM,0);
	if(network_conn<0){
		printf("socket failure\n");
		return -1;
	}

	if(connect(network_conn, (struct sockaddr*)&servaddr, sizeof(servaddr))!=0) {
		printf("connection failure\n");
		return -1;
	}

	//succefully connected
	printf("Opening connection to: %d\n", network_conn);
	return network_conn;
}

int PTPConnClose(int conn)
{
	printf("Closing connection %d\n", conn);
	close(conn);
	return 1;
}

int PTPDownloadRequestSuccess(dl_thread_param_t *param)
{
	// if valid download table
	printf("Downloading thread starting\n");
	// connect to peer up here, then start download thread

	usleep(100);	// wait for upload thread to open. probably not necessary

	// create download entry in peer table
	//PTPCreateDownloadEntry(peer_peer_t *table, peer_peer_t *node);

	// create stuff to send
	//! this comes as an argument!
	int file_size = param->file_size;
	printf("file_size: %d\n", file_size);
	char *file_name = param->file_name;

	char host_name[FILE_NAME_LEN];
	strcpy(host_name, param->host_name[0]);


	int num_dl_peers = param->num_dl_peers;
	printf("peers: %d\n", num_dl_peers);

	// use host_name to connect to other peer

	dl_param_t *download_info = calloc(num_dl_peers, sizeof(dl_param_t));
	// then partition file into n copies (seq_num and file_size bytes)
	pthread_t *download_thread = calloc(num_dl_peers, sizeof(pthread_t));

	int seq_num = 0;
	int seg_len = ceil( ((double)file_size) / ((double)num_dl_peers) ) ;	// probably need math.h here
	printf("seg_len: %d\n", seg_len);
	int listening_conn;

	// allocate memory for temporary file to hold file
	//http://www.gnu.org/software/libc/manual/html_node/File-Size.html -> file size maintained automatically
	char *file_name_tmp = calloc(1, 100);
	strcpy(file_name_tmp, file_name);
	strcat(file_name_tmp, "_tmp");
	FILE *fp=fopen(file_name_tmp, "w+");
	char *str_tmp = "\0";
	fwrite(str_tmp,1,1,fp);

	//printf("file name: %s\n", file_name);
	//printf("file name temp: %s\n", file_name_tmp);

	//! need host_name array
	//! count number of dl_peers

	// mutex for file pointer writing
	if (pthread_mutex_init(&write_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

	int i;
	for (i=0; i<num_dl_peers; i++){
		printf("connecting to server\n");
		listening_conn = PTPConnOpen(param->host_name[i]);
		if (listening_conn < 0){
			printf("failed to connect to host: %d!!\n", listening_conn);
			free(download_info);
			free(download_thread);
			free(file_name_tmp);
			fclose(fp);
			return -1;
		}

		download_info[i].sockfd = listening_conn;
		download_info[i].fp = fp;	// file pointer to allocated space for download
		strcpy(download_info[i].file_name, file_name);

		// start with the entire file first
		if (seq_num + seg_len < file_size){
			download_info[i].seq_num = seq_num;
			download_info[i].seg_len = seg_len;
			seq_num += seg_len;
		}else{
			download_info[i].seq_num = seq_num;
			download_info[i].seg_len = file_size - seq_num;

			if (i != num_dl_peers - 1){
				printf("ERROR IN CALCULATING DOWNLOAD PEERS\n");
				exit(EXIT_FAILURE);
			}
		}
        
		// needs peer_conn, seq_num, file_size, file_name
		printf("creating thread for download peer 1: %d\n", i );
		pthread_create(&download_thread[i], NULL, PTPDownload, &download_info[i]);
	}

	// if time out is reached, we done -> // what if a thread fails unexpectedly as we are downloading?
	// start timeout thread here

	/*
	pthread_t download_timeout_thread;
	pthread_create(download_timeout_thread, NULL, DownloadTimeout, NULL);
	*/

	for (i=0; i<num_dl_peers; i++){
		// pthread_join all of them
		pthread_join(download_thread[i], NULL);	//! syntax probably incorrect here
	}

	pthread_mutex_destroy(&write_lock);
	free(download_info);
	free(download_thread);
	free(file_name_tmp);

	fclose(fp);
	close(listening_conn);

	/*
	if (download_timeout == 0)
		return -1;	// download timeout fail
	*/
	// if all threads terminate successfully, we are done downloading
	printf("File done downloading!\n");

	return 1;
}

int PTPDownloadRequestSuccessFile(filetable_entry_t *file_entry)
{
	// if valid download table
	printf("Downloading thread starting for file %s\n",file_entry->fileInfo.filename);
	// connect to peer up here, then start download thread

	sleep(1);	// wait for upload thread to open. probably not necessary

	// create download entry in peer table

	// create stuff to send
	//! this comes as an argument!
	int file_size = file_entry->fileInfo.size;
	printf("file_size: %d\n", file_size);
	char *file_name = file_entry->fileInfo.filename;

	int num_dl_peers = file_entry->peerNum;

	// use host_name to connect to other peer

	dl_param_t *download_info = calloc(num_dl_peers, sizeof(dl_param_t));
	// then partition file into n copies (seq_num and file_size bytes)
	pthread_t *download_thread = calloc(num_dl_peers, sizeof(pthread_t));

	int seq_num = 0;
	int seg_len = ceil( ((double)file_size) / ((double)num_dl_peers) ) ;	// probably need math.h here
	printf("seg_len: %d\n", seg_len);
	int listening_conn;

	// allocate memory for temporary file to hold file
	//http://www.gnu.org/software/libc/manual/html_node/File-Size.html -> file size maintained automatically
	char *file_name_tmp = calloc(1, 100);
	strcpy(file_name_tmp, file_name);
	strcat(file_name_tmp, "_tmp");
	FILE *fp=fopen(file_name_tmp, "w+");
	char *str_tmp = "\0";
	fwrite(str_tmp,1,1,fp);

	// mutex for file pointer writing
	if (pthread_mutex_init(&write_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

	int i;
	for (i=0; i<num_dl_peers; i++){
		printf("connecting to server\n");
		listening_conn = PTPConnOpenFile(file_entry->peerList[i].addr);
		printf("addr: %s\n", inet_ntoa(file_entry->peerList[i].addr));
		printf("listening_conn: %d\n", listening_conn);
		//new adde !!!!!!
		usleep(10);
		if (listening_conn < 0){
			printf("failed to connect to host: %d!!\n", listening_conn);
			free(download_info);
			free(download_thread);
			free(file_name_tmp);
			fclose(fp);
			return -1;
		}

		printf("creating thread for download peer 1: %d\n", i );

		download_info[i].sockfd = listening_conn;
		download_info[i].fp = fp;	// file pointer to allocated space for download
		strcpy(download_info[i].file_name, file_name);

		// start with the entire file first
		if (seq_num + seg_len < file_size){
			download_info[i].seq_num = seq_num;
			download_info[i].seg_len = seg_len;
			seq_num += seg_len;
		}else{
			download_info[i].seq_num = seq_num;
			download_info[i].seg_len = file_size - seq_num;
		}
		// needs peer_conn, seq_num, file_size, file_name
		pthread_create(&download_thread[i], NULL, PTPDownload, &download_info[i]);
	}

	// if time out is reached, we done -> // what if a thread fails unexpectedly as we are downloading?
	// start timeout thread here

	for (i=0; i<num_dl_peers; i++){
		// pthread_join all of them
		pthread_join(download_thread[i], NULL);	//! syntax probably incorrect here
	}

	pthread_mutex_destroy(&write_lock);
	free(download_info);
	free(download_thread);

	fclose(fp);

	//rename file upon success
    char command[150];
    sprintf(command,"mv %s %s", file_name_tmp, file_name);
    system(command);

    //touch new time for file_name
    char date[16];
    time_t time_last_modified = file_entry->fileInfo.timestamp - 14400;
    strftime(date,16,"%Y%m%d%H%M.%S",gmtime(&time_last_modified));
    char command2[150];
    sprintf(command2,"touch -t %s %s", date, file_name);
    system(command2);

	free(file_name_tmp);
	close(listening_conn);

	// if all threads terminate successfully, we are done downloading
	printf("File done downloading!\n");

	return 1;
}

void* DownloadTimeout(void *arg)
{
	// check global var
	nanosleep((struct timespec[]){{0, DOWNLOAD_TIMEOUT_NS}}, NULL);
	// nanosleep 
	printf("Download timeout!\n");
	download_timeout = 0;
	pthread_exit(NULL);
}


int PTPSendFile(int peer_conn, partition_t *data)
{
	if (send(peer_conn, data, sizeof(partition_t), 0) < 0){
		printf("PTP Send Error\n");
		printf("Partition data information: \n");
		printf("	type: %d\n", data->header.type);
		printf("	seq_num: %lld\n", data->header.seq_num);
		printf("	length: %lld\n", data->header.length);
		printf("	file_name: %s\n", data->header.file_name);
		printf("	peer_conn: %d\n", peer_conn);

		return -1;
	}
	return 1;
}

// receives file from peer
// stores into correct slot based on partition number
int PTPRecvFile(int peer_conn, partition_t *data_out)
{
	memset(data_out,0,sizeof(partition_t));
	int length = sizeof(partition_t);
	int cur = 0;
	while (cur < length){
		int size = cur + 1024 <= length ? 1024 : length - cur;
		int result = recv(peer_conn, ((char*)data_out)+cur, size, 0);
		if(result < 1) return -1;
		cur += result;
	}
	return 1;
}
