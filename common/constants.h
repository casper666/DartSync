/*
* Modified: Jamie Yang 5/20/15
*/

//global constants
#define MAX_HOSTNAME 128
#define ALIVE_TIME 40

//peer.h constants
#define IP_LEN 40
#define FILE_NAME_LEN 50	// security risk here. file_name uses strncpy, so if file name == 50 there will be no null terminator
#define PROTOCOL_LEN 50
#define RESERVED_LEN 50
#define MAX_SEG_LEN  1464	//MAX_SEG_LEN = 1500 - sizeof(seg header) - sizeof(ip header)

//peer.c constants
#define HANDSHAKE_WAIT 500000000
#define MAX_TABLE_LEN 102400

//p2p.h constants
#define MAX_DATA_LEN 10240	// max download segment length
#define DOWNLOAD_TIMEOUT_NS 1000000000 //DOWNLOAD_TIMEOUT value in nano seconds (10 s rn)
#define MAX_PEER_THREADS 10
#define MAX_FILE_LEN 999999 // something big
#define MAX_CONCUR_DOWNLOADS 10 // number of maximum downloads each peer is allowed to have at once
#define SEND_WAIT_NS 50000000
#define RES_DEF 0

//filetable.h constants
#define MAX_PEER_NUMBER 10
#define NONE 0
#define ADD 1
#define DELETE 2
#define MODIFY 3
#define INIT 0
#define UPDATE 1
#define PIECE_LENGTH 99
#define NORMALFILE 8
#define FOLDER 4

#define CONNECTION_PORT 2987 //connect peer to tracker
#define LISTENING_PORT 3367 //peer's p2p listening port

#define HANDSHAKE_PORT 5729 //peer to do handshake on?


//protocol constants
#define REGISTER -1
#define KEEP_ALIVE 1
#define FILE_UPDATE 2
