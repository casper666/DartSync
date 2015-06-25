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
#include "resume_dl.h"

// global variable declarations
const char extension[] = "_res";
pthread_mutex_t write_lock;

// functions


// reads in a res_file if one exists & checks to see if is valid
// if invalid, just download the whole file

res_file_t* ReadResFile(char *file_name){
	// if doesn't exist, do nothing
	if (CheckResFile(file_name) < 0)
		return NULL;

	char *res_file_name = calloc(1, strlen(extension) + 1 + MAX_FILE_LEN);
	strcpy(res_file_name, file_name);
	strcat(res_file_name, extension);

	// if exists, return the res_file table
	FILE *fp;
	if ( (fp = fopen(res_file_name, "r")) == NULL){
		printf("Invalid res file\n");
		free(res_file_name);
		return NULL;
	}

	res_file_t* read_in = calloc(1, sizeof(res_file_t));
	strcpy( read_in->file_name, file_name);

	int i=0;
	while (!feof(fp)){
		if (i == MAX_PEER_NUMBER){
			printf("Error in res file\n");
			free(read_in);
			free(res_file_name);
			return NULL;
		}

		unsigned long long int tmp1;
		unsigned long long int tmp2;
		unsigned long long int tmp3;

		fscanf(fp, "%llu %llu %llu\n", &tmp1, &tmp2, &tmp3); // seq num, dl len, total seg len

		if (tmp1+tmp2+tmp3!=0){
			read_in->segment[0][i]=tmp1;
			read_in->segment[1][i]=tmp2;
			read_in->segment[2][i]=tmp3;
			printf("seq num: %llu, dl len: %llu, total len, %llu\n", read_in->segment[0][i], read_in->segment[1][i], read_in->segment[2][i]);
			read_in->num_segs++;
			i++;
		}
	}
	
	if ( CheckArgs(read_in) < 0){
		printf("Invalid res file\n");
		DeleteResFile(res_file_name);
		free(read_in);
		free(res_file_name);
		return NULL;
	}

	free(res_file_name);
	return read_in;
}

// checks to see if downloaded file has a corresponding .res file
int CheckResFile(char *file_name){
	// check to see if file_name + _res exists
	char *res_file_name = calloc(1, strlen(extension) + 1 + MAX_FILE_LEN);
	strcpy(res_file_name, file_name);
	strcat(res_file_name, extension);

	if (access(res_file_name, F_OK) != -1){
		free(res_file_name);
		return 1;
	}

	free(res_file_name);
	return -1;
}

// checks to see if .res file inputs are valid
int CheckArgs(res_file_t *check){
	unsigned int num_segs = check->num_segs;
	if (num_segs == 0 || num_segs > MAX_PEER_NUMBER){
		printf("Res_table peer number invalid!\n");
		return -1;
	}

	char *file_name = check->file_name;
	if (file_name == NULL){
		printf("Res_Table file name invalid!\n");
		return -1;
	}

	if (access(file_name, F_OK) == -1){
		printf("File in res_table does not exist!\n");
		return 1;
	}

	FILE *f;
	f = fopen(file_name,"r+");
	if (f == NULL){
		printf("fopen error\n");
		exit(EXIT_FAILURE);
	}
	fseek(f,0,SEEK_END);
	int fileLen = ftell(f);
	fclose(f);

	int i;
	for (i=0; i < MAX_PEER_NUMBER; i++){
		if ( i > num_segs - 1){
			if (check->segment[0][i] != 0 || check->segment[1][i] != 0){
				printf("Res_table segment invalid!\n");
				return -1;
			}
		}
		if ( (check->segment[2][i]) > fileLen ){
			printf("Download request exceeds file length\n");
			return -1;
		}

		if ( check->segment[1][i] > check->segment[2][i] ){
			printf("Download request exceeds file length\n");
			return -1;
		}

		if( check->segment[0][i] + check->segment[1][i] + check->segment[2][i] == 0)
			break;
	}

	return 1;
}

// creates new .res file
int UpdateResFile(res_file_t input){
	char *file_name = input.file_name;
	char *res_file_name = calloc(1, strlen(extension) + 1 + MAX_FILE_LEN);
	strcpy(res_file_name, file_name);
	strcat(res_file_name, extension);

	FILE* fp;

	if ( (fp = fopen(res_file_name,"a")) == NULL){
		printf("Failed to open res_file in creating it\n");
		free(res_file_name);
		return -1;
	}

	fprintf(fp, "%lld %lld %lld\n", input.segment[0][0] , input.segment[1][0] , input.segment[2][0] );

	fclose(fp);
	free(res_file_name);
	return 1;
}

// deletes .res file upon successful download
int DeleteResFile(char *file_name){
	char *res_file_name = calloc(1, strlen(extension) + 1 + MAX_FILE_LEN);
	strcpy(res_file_name, file_name);
	strcat(res_file_name, extension);

	unlink(res_file_name);

	free(res_file_name);
	return 1;
}

int PTPDownloadRequestSuccessRes(dl_thread_param_t *param){
	// if valid download table
	printf("Downloading thread starting\n");
	// connect to peer up here, then start download thread

	sleep(1);	// wait for upload thread to open. probably not necessary

	// create stuff to send
	//! this comes as an argument!
	int file_size = param->file_size;
	printf("file_size: %d\n", file_size);
	char *file_name = param->file_name;

	char host_name[FILE_NAME_LEN];
	strcpy(host_name, param->host_name[0]);

	int num_dl_peers = param->num_dl_peers;
	printf("peers: %d\n", num_dl_peers);

	// then partition file into n copies (seq_num and file_size bytes)
	pthread_t *download_thread = calloc(num_dl_peers, sizeof(pthread_t));

	int seq_num = 0;
	int seg_len = ceil( ((double)file_size) / ((double)num_dl_peers) ) ;	// probably need math.h here
	printf("seg_len: %d\n", seg_len);
	int listening_conn;

	// allocate memory for temporary file to hold file
	//http://www.gnu.org/software/libc/manual/html_node/File-Size.html -> file size maintained automatically
	int tmp_exists = 0;
	char *file_name_tmp = calloc(1, 100);
	strcpy(file_name_tmp, file_name);
	strcat(file_name_tmp, "_tmp");
	FILE *fp;
	if (access(file_name_tmp, F_OK) != -1){
		tmp_exists = 1;
		printf("EDIT MODE\n");
		fp=fopen(file_name_tmp, "r+");
	}else{
		printf("WRITE MODE\n");
		fp=fopen(file_name_tmp, "w+");
	}

	// mutex for file pointer writing
	if (pthread_mutex_init(&write_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return -1;
    }

    // check to see if we are resuming the download or starting fresh
    res_file_t *res_table = ReadResFile(file_name);
    unsigned int num_segs;
    int remain_segs;	// used for resume, in the case that the number of resume peers is less than the number of download segments
    if (res_table != NULL){
    	//res_table = ReadResFile(file_name);
    	if (!tmp_exists){
    		free(res_table);
    		res_table = NULL;
    	}else{
    		num_segs = res_table->num_segs;
	    	remain_segs = num_segs;
    	}
    	DeleteResFile(file_name);
    }else{
    	// handle logic for existing _res file without corresponding _tmp file
    	if (tmp_exists){
    		char *tmp_file_name = calloc(1, strlen("_tmp") + 1 + MAX_FILE_LEN);
			strcpy(tmp_file_name, file_name);
			strcat(tmp_file_name, "_tmp");
			unlink(tmp_file_name);
			fp = fopen(tmp_file_name,"w");
			free(tmp_file_name);
    	}
    }

	res_dl_param_t *download_info = calloc(num_dl_peers, sizeof(res_dl_param_t));

	int num_join_peers = 0;		// number of pthread_joins

	// logic to start download threads
	int i;
	for (i=0; i<num_dl_peers; i++){
		printf("connecting to server\n");

		// open connection (if statement checks to see if on resume, the number of dl peers exceeds the number of dl segments)
		if ( !( res_table != NULL && (num_dl_peers > num_segs) && (i >= num_segs) ) ){
			listening_conn = PTPConnOpen(param->host_name[i]);
			if (listening_conn < 0){
				printf("failed to connect to host: %d!!\n", listening_conn);
				free(download_info);
				free(download_thread);
				free(file_name_tmp);
				fclose(fp);
				return -1;
			}
		}
		
		// Fill in information passed to download thread
		download_info[i].sockfd = listening_conn;
		download_info[i].fp = fp;	// file pointer to allocated space for download
		strcpy(download_info[i].file_name, file_name);
		download_info[i].total_file_len  = file_size;
		download_info[i].total_num_peers = num_dl_peers;
		
		if (res_table == NULL){
			// if we are not resuming, do this
			download_info[i].num_dl_on_resume = 1;	// each file downloads one segment
			if (seq_num + seg_len < file_size){
				download_info[i].seq_num[0] = seq_num;
				download_info[i].seg_len[0] = seg_len;
				seq_num += seg_len;
			}else{
				download_info[i].seq_num[0] = seq_num;
				download_info[i].seg_len[0] = file_size - seq_num;

				if (i != num_dl_peers - 1){
					printf("ERROR IN CALCULATING DOWNLOAD PEERS\n");
					exit(EXIT_FAILURE);
				}
			}
			printf("creating thread for download peer 1: %d\n", i );
			pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
			num_join_peers++;
		}else{
			// res_file exists, lets goo!
			//unsigned long long int seq_num_res = res_table->segment[0][i];
			unsigned long long int seg_len_res = res_table->segment[1][i];
			unsigned long long int total_len_res = res_table->segment[2][i];

			if (num_dl_peers == num_segs){
				// case 1: num_dl_peers == num_segs
				download_info[i].num_dl_on_resume = 1;
				download_info[i].seq_num[0] = seg_len_res;
				download_info[i].seg_len[0] = total_len_res - seg_len_res;
				num_join_peers++;
				//sleep(2);
				pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
			}else if(num_dl_peers > num_segs){
				// case 2: num_dl_peers > num_segs
				if (i < num_segs){
					download_info[i].num_dl_on_resume = 1;
					download_info[i].seq_num[0] = seg_len_res;
					download_info[i].seg_len[0] = total_len_res - seg_len_res;	
					num_join_peers++;
					//sleep(2);
					pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
				}
			}else{
				// case 3: num_dl_peers < num_segs
				// loop and distribute total seg number among peers, setting num_dl_on_resume accordingly
				int max_partition = ceil( ((double)num_segs) / ((double)num_dl_peers) ) ;	// probably need math.h here
				int partition_used;	// how many partitions are used for this segment?

				if (remain_segs < max_partition){
					partition_used = remain_segs;
				}else{
					partition_used = max_partition;
				}

				download_info[i].num_dl_on_resume = partition_used;	// each file downloads possibly more than one segment
				int j;
				for (j=0; j < partition_used; j++){
					download_info[i].seq_num[j] = res_table->segment[1][i + j];
					download_info[i].seg_len[j] = res_table->segment[2][i + j] - res_table->segment[1][i + j];
				}

				num_join_peers++;
				//sleep(2);
				pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
				remain_segs -= partition_used;
			}
		}
	}

	for (i=0; i<num_join_peers; i++){
		// pthread_join all of them
		pthread_join(download_thread[i], NULL);	//! syntax probably incorrect here
	}

	pthread_mutex_destroy(&write_lock);
	free(download_info);
	free(download_thread);
	free(file_name_tmp);
	free(res_table);

	fclose(fp);
	close(listening_conn);

	// if _res file exists, then we didn't complete the download

/*
	if (access(res_file_name, F_OK) != -1){
		printf("Res table file created, download NOT finished. Reconnect peers to resume downloading\n");
		// don't convert _tmp file to regular file name

	}else{
		printf("File done downloading!\n");
		// convert _tmp file to other file name

	}
*/
	// if all threads terminate successfully, we are done downloading

	return 1;
}

int PTPDownloadRequestSuccessFileRes(filetable_entry_t *file_entry){
	// if valid download table
	printf("Downloading thread starting\n");
	// connect to peer up here, then start download thread

	sleep(1);	// wait for upload thread to open. probably not necessary

	// create stuff to send
	//! this comes as an argument!
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

	// then partition file into n copies (seq_num and file_size bytes)
	pthread_t *download_thread = calloc(num_dl_peers, sizeof(pthread_t));

	int seq_num = 0;
	int seg_len = ceil( ((double)file_size) / ((double)num_dl_peers) ) ;	// probably need math.h here
	printf("seg_len: %d\n", seg_len);
	int listening_conn;

	// allocate memory for temporary file to hold file
	//http://www.gnu.org/software/libc/manual/html_node/File-Size.html -> file size maintained automatically
	int tmp_exists = 0;
	char *file_name_tmp = calloc(1, MAX_FILE_LEN + 4 + 1);
	strcpy(file_name_tmp, file_name);
	strcat(file_name_tmp, "_tmp");
	FILE *fp;
	if (access(file_name_tmp, F_OK) != -1){
		tmp_exists = 1;
		printf("EDIT MODE\n");
		fp=fopen(file_name_tmp, "r+");
	}else{
		printf("WRITE MODE\n");
		fp=fopen(file_name_tmp, "w+");
	}

	// mutex for file pointer writing
	if (pthread_mutex_init(&write_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return -1;
    }

    // check to see if we are resuming the download or starting fresh
    res_file_t *res_table = ReadResFile(file_name);
    unsigned int num_segs;
    int remain_segs;	// used for resume, in the case that the number of resume peers is less than the number of download segments
    if (res_table != NULL){
    	//res_table = ReadResFile(file_name);
    	if (!tmp_exists){
    		free(res_table);
    		res_table = NULL;
    	}else{
    		num_segs = res_table->num_segs;
	    	remain_segs = num_segs;
    	}
    	DeleteResFile(file_name);
    }else{
    	// handle logic for existing _res file without corresponding _tmp file
    	if (tmp_exists){
    		char *tmp_file_name = calloc(1, strlen("_tmp") + 1 + MAX_FILE_LEN);
			strcpy(tmp_file_name, file_name);
			strcat(tmp_file_name, "_tmp");
			unlink(tmp_file_name);
			fp = fopen(tmp_file_name,"w");
			free(tmp_file_name);
    	}
    }

	res_dl_param_t *download_info = calloc(num_dl_peers, sizeof(res_dl_param_t));

	int num_join_peers = 0;		// number of pthread_joins

	// logic to start download threads
	int i;
	for (i=0; i<num_dl_peers; i++){
		printf("connecting to server\n");

		// open connection (if statement checks to see if on resume, the number of dl peers exceeds the number of dl segments)
		if ( !( res_table != NULL && (num_dl_peers > num_segs) && (i >= num_segs) ) ){
			listening_conn = PTPConnOpenFile(file_entry->peerList[i].addr);
			printf("addr: %s\n", inet_ntoa(file_entry->peerList[i].addr));
			printf("listening_conn: %d\n", listening_conn);
			usleep(10);
			if (listening_conn < 0){
				printf("failed to connect to host: %d!!\n", listening_conn);
				free(download_info);
				free(download_thread);
				free(file_name_tmp);
				fclose(fp);
				return -1;
			}
		}
		
		// Fill in information passed to download thread
		download_info[i].sockfd = listening_conn;
		download_info[i].fp = fp;	// file pointer to allocated space for download
		strcpy(download_info[i].file_name, file_name);
		download_info[i].total_file_len  = file_size;
		download_info[i].total_num_peers = num_dl_peers;
		
		if (res_table == NULL){
			// if we are not resuming, do this
			download_info[i].num_dl_on_resume = 1;	// each file downloads one segment
			if (seq_num + seg_len < file_size){
				download_info[i].seq_num[0] = seq_num;
				download_info[i].seg_len[0] = seg_len;
				seq_num += seg_len;
			}else{
				download_info[i].seq_num[0] = seq_num;
				download_info[i].seg_len[0] = file_size - seq_num;

				// if (i != num_dl_peers - 1){
				// 	printf("ERROR IN CALCULATING DOWNLOAD PEERS\n");
				// 	exit(EXIT_FAILURE);
				// }
			}
			printf("creating thread for download peer 1: %d\n", i );
			pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
			num_join_peers++;
		}else{
			// res_file exists, lets goo!
			//unsigned long long int seq_num_res = res_table->segment[0][i];
			unsigned long long int seg_len_res = res_table->segment[1][i];
			unsigned long long int total_len_res = res_table->segment[2][i];

			if (num_dl_peers == num_segs){
				// case 1: num_dl_peers == num_segs
				download_info[i].num_dl_on_resume = 1;
				download_info[i].seq_num[0] = seg_len_res;
				download_info[i].seg_len[0] = total_len_res - seg_len_res;
				num_join_peers++;
				//sleep(2);
				pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
			}else if(num_dl_peers > num_segs){
				// case 2: num_dl_peers > num_segs
				if (i < num_segs){
					download_info[i].num_dl_on_resume = 1;
					download_info[i].seq_num[0] = seg_len_res;
					download_info[i].seg_len[0] = total_len_res - seg_len_res;	
					num_join_peers++;
					//sleep(2);
					pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
				}
			}else{
				// case 3: num_dl_peers < num_segs
				// loop and distribute total seg number among peers, setting num_dl_on_resume accordingly
				int max_partition = ceil( ((double)num_segs) / ((double)num_dl_peers) ) ;	// probably need math.h here
				int partition_used;	// how many partitions are used for this segment?

				if (remain_segs < max_partition){
					partition_used = remain_segs;
				}else{
					partition_used = max_partition;
				}

				download_info[i].num_dl_on_resume = partition_used;	// each file downloads possibly more than one segment
				int j;
				for (j=0; j < partition_used; j++){
					download_info[i].seq_num[j] = res_table->segment[1][i + j];
					download_info[i].seg_len[j] = res_table->segment[2][i + j] - res_table->segment[1][i + j];
				}

				num_join_peers++;
				//sleep(2);
				pthread_create(&download_thread[i], NULL, PTPDownloadRes, &download_info[i]);
				remain_segs -= partition_used;
			}
		}
	}

	for (i=0; i<num_join_peers; i++){
		// pthread_join all of them
		pthread_join(download_thread[i], NULL);	//! syntax probably incorrect here
	}

	// if _res file exists, then we didn't complete the download
	char *res_file_name = calloc(1, MAX_FILE_LEN + 4 + 1);
	strcpy(res_file_name, file_name);
	strcat(res_file_name, "_res");

	if (access(res_file_name, F_OK) != -1){
		printf("Res table file created, download NOT finished. Reconnect peers to resume downloading\n");
	}else{
		printf("File done downloading!\n");
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
	}

	pthread_mutex_destroy(&write_lock);
	free(download_info);
	free(download_thread);
	free(file_name_tmp);
	free(res_table);

	fclose(fp);
	close(listening_conn);
	free(res_file_name);

	// if all threads terminate successfully, we are done downloading

	return 1;
}

// when a download thread is created, the download thread takes in an argument that contains the file to be downloaded and the file length
// downloader is in charge of requesting segments from the uploader
// each thread will request different segments. how do we centralize this process? send as arguments the segments needed?
void* PTPDownloadRes(void* arg)
{
	// parse argument info and send message to uploader containing sequence number, file name, file length, peer, IP, etc.
	res_dl_param_t *dl_args = (res_dl_param_t *) arg;

	// read in dl_args that don't need to be looped
	int num_dl_on_resume = dl_args->num_dl_on_resume;
	int peer_conn = dl_args->sockfd;	// comes from argument
	char *file_name = dl_args->file_name;
	FILE *fp = dl_args->fp;

	int seg_itr;
	for (seg_itr = 0; seg_itr < num_dl_on_resume; seg_itr ++){
		// send connection request to peer by looking up IP table
		unsigned long long int seq_num_thread = dl_args->seq_num[seg_itr];
		printf("download sequence number: %lld\n", seq_num_thread);
		unsigned long long int length_thread = dl_args->seg_len[seg_itr];
		printf("length of thread: %lld\n", length_thread);

		int downloading = 1;
		unsigned long long int recv_buf = 0;

		if(length_thread == 0){
			continue;
		}

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
		if (PTPSendFile(peer_conn, &data) < 0){
			printf("PTPSendFile err\n");
			return NULL;
		}

		int last_seq_num = 0;

		while(downloading){
			if (RES_DEF)
				usleep(10000);

			partition_t data_out;
			memset(&data_out, 0, sizeof(partition_t) );

			if (PTPRecvFile(peer_conn, &data_out) < 0){
				printf("Receive file error\n");
				pthread_mutex_lock(&write_lock);
				res_file_t write_res;
				memset(&write_res, 0, sizeof(res_file_t) );
				write_res.num_segs = dl_args->total_file_len;
				strcpy(write_res.file_name, file_name);
				write_res.segment[0][seg_itr] = seq_num_thread;			// start seq_num
				write_res.segment[1][seg_itr] = last_seq_num;			// amount downloaded
				write_res.segment[2][seg_itr] = length_thread + seq_num_thread;			// length of total segment
				if (UpdateResFile(write_res) < 0){
					printf("~~~~Failed to update resume file!\n");
				}
				pthread_mutex_unlock(&write_lock);
				break;
			}


			if (data_out.header.type == UPLOADING){
				printf("Received data from peer[%d]: %lld/%lld\n", peer_conn, seq_num_thread + data_out.header.seq_num , length_thread + seq_num_thread);
				//printf("%s", data_out.data);

				// mutex this write operation
				pthread_mutex_lock(&write_lock);

				fseek(fp, seq_num_thread + data_out.header.seq_num , SEEK_SET);
				printf("seq num write: %lld\n", seq_num_thread + data_out.header.seq_num);
				fwrite(data_out.data , data_out.header.length, 1, fp);
				fseek(fp, 0 , SEEK_CUR);
				fflush(fp);

				// update resume file here (or at least resume data struct)

				// can do if sig_int received THEN do this?
				// if control C
				// logic here to add download resume
				int not_yet = 0;
				if (not_yet){
					res_file_t write_res;
					memset(&write_res, 0, sizeof(res_file_t) );
					write_res.num_segs = dl_args->total_file_len;
					strcpy(write_res.file_name, file_name);
					write_res.segment[0][seg_itr] = seq_num_thread;						// start seq_num
					write_res.segment[1][seg_itr] = recv_buf;							// amount downloaded
					write_res.segment[2][seg_itr] = length_thread + seq_num_thread;		// length of total segment
					if (UpdateResFile(write_res) < 0){
						printf("~~~~Failed to update resume file!\n");
					}
				}

				//last_seq_num = data_out.header.seq_num + data_out.header.length;
				recv_buf += data_out.header.length;
				last_seq_num = seq_num_thread + data_out.header.seq_num + data_out.header.length;

				pthread_mutex_unlock(&write_lock);

				if (recv_buf == length_thread){
					printf("received buffer equals length of segment requested\n");
					downloading = 0;
				}else if(recv_buf > length_thread){
					// debug
					printf("Buffer calculation error!\n");
					exit(EXIT_FAILURE);
				}

			}else{
				// download resume 
				printf("Download thread fail\n");
				pthread_mutex_lock(&write_lock);
				res_file_t write_res;
				memset(&write_res, 0, sizeof(res_file_t) );
				write_res.num_segs = dl_args->total_file_len;
				strcpy(write_res.file_name, file_name);
				write_res.segment[0][seg_itr] = seq_num_thread;							// start seq_num
				write_res.segment[1][seg_itr] = last_seq_num;							// amount downloaded
				write_res.segment[2][seg_itr] = length_thread + seq_num_thread;			// length of total segment
				if (UpdateResFile(write_res) < 0){
					printf("~~~~Failed to update resume file!\n");
				}
				pthread_mutex_unlock(&write_lock);
				break;
			}
		}
	}


	partition_t data_finish; // read information from argument
	memset(&data_finish, 0 , sizeof(partition_t));
	data_finish.header.type = FINISH;
	if (PTPSendFile(peer_conn, &data_finish) < 0){
		printf("send file error!\n");
	}

	printf("Download thread complete, closing connection\n");
	PTPConnClose(peer_conn);
	pthread_exit(NULL);
}

// receives file from peer
// stores into correct slot based on partition number
int PTPRecvFile2(int peer_conn, partition_t *data_out)
{
	memset(data_out,0,sizeof(partition_t));
	int length = sizeof(partition_t);
	int cur = 0;
	while (cur < length){
		int size = cur + 1024 <= length ? 1024 : length - cur;
		int result = recv(peer_conn, ((char*)data_out)+cur, size, 0);
		if(result < 1) return -1;
		//printf("result of recv function is : %d, cur position : %d, sizeof(ptp_peer_t) is %lu\n", result, cur, sizeof(ptp_peer_t));
		//memcpy(data, (char*)&data_in+cur, 128);
		cur += result;
	}
	return 1;
}
