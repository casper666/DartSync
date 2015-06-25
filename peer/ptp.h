/*
* Modified: Jamie Yang 5/20/15
*/

// -------- header files
#include "../common/constants.h"
#include "../common/common.h"

// -------- constants
#define UPLOADING 1
#define DOWNLOADING 2
#define FINISH 3
#define FAIL 4

// -------- structure definitions

//P2P Downloading/Uploading File Message
// uploading side will send a seq_num 
typedef struct prt_hdr {
	char file_name[FILE_NAME_LEN];
	unsigned int src_port;        //source port number
	unsigned int dest_port;       //destination port number
	unsigned long long int seq_num;         //sequence number of file used for partitioning
	unsigned long long int length;    //total data length
	unsigned short int  type;     //uploading == 1 or downloading == 2
} prt_hdr_t;

typedef struct partition {
	prt_hdr_t header;
	char data[MAX_DATA_LEN];
} partition_t;

//download file parameters
typedef struct dl_param{
	unsigned long long int seq_num;			// start number for sequence
	unsigned long long int seg_len;			// length of partition for this thread
	int sockfd;								// connecting sockfd
	char file_name[FILE_NAME_LEN];			// file name with directory
	FILE *fp;								// file pointer to allocated memory in disk that is total length
}dl_param_t;

typedef struct dl_thread_param{
	int file_size;			// contains size of file
	char file_name[FILE_NAME_LEN];
	char *host_name [MAX_PEER_NUMBER];
	int num_dl_peers;
	int peer_list;			// should actually be the actual peer list
}dl_thread_param_t;

// -------- function prototypes
void* PTPListen(void* arg);
void* PTPUpload(void* arg);
void* PTPDownload(void* arg);	// takes in dl_param_t
char* GetFileData(char *file_name, unsigned long long int seq_num, unsigned long long int file_len);
int PTPDownloadRequestSuccess(dl_thread_param_t *param);
int PTPDownloadRequestSuccessFile(filetable_entry_t *file_entry);

int PTPConnOpen(char *hostname);
int PTPConnOpenFile(struct in_addr addr);

int PTPConnClose(int conn);

int PTPSendFile(int peer_conn, partition_t *data);

int PTPRecvFile(int peer_conn, partition_t *data);

char* FileGetPartition(int length, int seq_num, char* file);
