/*
* Modified: Jamie Yang 5/20/15
*/

// -------- header files
#include "../common/constants.h"
#include "../common/common.h"

// -------- constants
typedef struct res_file{
	unsigned int num_segs;
	char file_name[FILE_NAME_LEN];
	unsigned long long int segment [3][MAX_PEER_NUMBER];
} res_file_t;

typedef struct res_dl_param{
	// download informatin
	unsigned long long int seq_num[MAX_PEER_NUMBER];			// start number for sequence
	unsigned long long int seg_len[MAX_PEER_NUMBER];			// length of partition for this thread
	int sockfd;								// connecting sockfd
	char file_name[FILE_NAME_LEN];			// file name with directory
	FILE *fp;								// file pointer to allocated memory in disk that is total length

	// resume information
	unsigned long long int total_file_len;						// used for resume
	unsigned long long int total_num_peers;						// used for resume
	int num_dl_on_resume;					// number of download segments on resume
}res_dl_param_t;

// -------- structure definitions

// -------- function prototypes

// reads in a res_file if one exists
res_file_t* ReadResFile(char *file_name);

// creates new .res file
int CreateResFile(res_file_t input);

// for each download, update the res file
int UpdateResFile(res_file_t update);

// deletes .res file upon successful download
int DeleteResFile(char *file_name);

// checks to see if downloaded file has a corresponding .res file
int CheckResFile(char *file_name);

// checks to see if .res file inputs are valid
int CheckArgs(res_file_t* check);

int PTPDownloadRequestSuccessRes(dl_thread_param_t *param);			// test function
int PTPDownloadRequestSuccessFileRes(filetable_entry_t *file_entry);// working function

void* PTPDownloadRes(void* arg);

int PTPRecvFile2(int peer_conn, partition_t *data_out);
