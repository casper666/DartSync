#include "../common/common.h"

int PTTSendFileT(int peer_conn, ptp_peer_t *data);

int TTPRecvFileT(int peer_conn, ptp_tracker_t *data);

char* tableToCharArray(filetable_t* fileTable);

filetable_t* charArrayTotable(char* tableArray, int fileNum);