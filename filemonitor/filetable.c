//FILE: filemonitor/filetable.h
//
//Description: this file implements the data structures and functions used by fileMonitor process
//
//Date: May 19,2015

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "filetable.h"
#include "../peer/ptp.h"
#include "../peer/resume_dl.h"

#define DEBUG 1

//global voriables
fileinfo_entry_t* head;
fileinfo_entry_t* pre;

//create filetable
filetable_t* create_filetable(){
	filetable_t* filetable = (filetable_t*)malloc(sizeof(filetable_t));
	memset(filetable,0,sizeof(filetable_t));
	filetable->filenumber = 0;
	filetable->isBlocked = 0;
	filetable->head = NULL;
	filetable->tail = NULL;
	return filetable;
}

//initialize filetable
int init_filetable(filetable_t* fileTable, fileinfo_entry_t* head, char* directory){
	if(fileTable->isBlocked) return 1;
	fileinfo_entry_t* cur = head;
	fileinfo_entry_t* prev = NULL;
	while(cur != NULL){
		//initialize filetable_entry
		filetable_entry_t* filetable_entry = (filetable_entry_t*)malloc(sizeof(filetable_entry_t));
		memset(filetable_entry,0,sizeof(filetable_entry_t));
		//get realtive directory
		memcpy(filetable_entry->fileInfo.filename,(char*)(cur->filename)+strlen(directory),strlen(cur->filename)-strlen(directory));
		//size of the file
		filetable_entry->fileInfo.size = cur->size;
		//timestamp of the file
		filetable_entry->fileInfo.timestamp = cur->timestamp;
		//type of the file
		filetable_entry->fileInfo.type = cur->type;
		add_filetable_entry(fileTable, filetable_entry);
		prev = cur;
		cur = cur->next;
		free(prev);
	}
	return 1;
}

int compare_filetable(filetable_t* fileTable, filetable_t* temp_fileTable){
	int isSame = 1;
	filetable_entry_t* head1 = fileTable->head;
	filetable_entry_t* head2 = temp_fileTable->head;
	//check if any file in head1 is deleted
	while(head1 != NULL){
		int deleteFlag = 1;
		while(head2 != NULL){
			if(!strcmp(head1->fileInfo.filename, head2->fileInfo.filename)){
				if(head2->fileInfo.timestamp != head1->fileInfo.timestamp){
					isSame = 0;
					if(head2->fileInfo.type == NORMALFILE){
						if(initDownload(head2)<0){
							fprintf(stderr, "Failed to init download \n");
							return -1;
						}
						printf("+++++++[modified]file %s has been downloaded by ptp+++++++\n",head2->fileInfo.filename);
					}
				}
				deleteFlag = 0;
				break;
			}
			head2 = head2->next;
		}
		if(deleteFlag){
			isSame = 0;
			if(head1->fileInfo.type == NORMALFILE){
				char command[50];
				sprintf(command,"rm %s",head1->fileInfo.filename);
				system(command);
				printf("+++++++file %s has been deleted by rm+++++++\n",head1->fileInfo.filename);
			}else{
				char command[50];
				sprintf(command,"rm -r %s",head1->fileInfo.filename);
				system(command);
				printf("+++++++directory %s has been deleted by rm+++++++\n",head1->fileInfo.filename);
			}
		}
		head1 = head1->next;
		head2 = temp_fileTable->head;
	}
	//check if there is any new file in head2
	head1 = fileTable->head;
	head2 = temp_fileTable->head;
	while(head2 != NULL){
		int addFlag = 1;
		while(head1 != NULL){
			if(!strcmp(head1->fileInfo.filename, head2->fileInfo.filename)){
				addFlag = 0;
        break;
			}
      head1 = head1->next;
    }
		if(addFlag){
			isSame = 0;
			if(head2->fileInfo.type == NORMALFILE){
				if(initDownload(head2)<0){
					fprintf(stderr, "Failed to init download \n");
					return -1;
				}
				printf("+++++++[added]file %s has been download by ptp+++++++\n",head2->fileInfo.filename);
			}else{
				char command1[50];
				sprintf(command1,"mkdir %s",head2->fileInfo.filename);
				system(command1);
    		char date[16];
    		time_t time_last_modified = head2->fileInfo.timestamp - 14400;
    		strftime(date,16,"%Y%m%d%H%M.%S",gmtime(&time_last_modified));
    		char command[150];
    		sprintf(command,"touch -t %s %s", date, head2->fileInfo.filename);
    		system(command);
        printf("+++++++[added]directory %s has been download by mkdir+++++++\n",head2->fileInfo.filename);
			}
		}
		head2 = head2->next;
    head1 = fileTable->head;
	}
  return isSame == 1 ? -1 : 1;
}

//given a new filetabl, update the local one
int update_filetable(filetable_t* fileTable, filetable_t* temp_fileTable, int mode){
	//block SIGNAL
	if(fileTable->isBlocked) return -1;
	//sameFlag is initialized as TRUE
	int isSame = 1;
	//check if any file need delete
	filetable_entry_t* head1 = fileTable->head;
	filetable_entry_t* head2 = temp_fileTable->head;
	while(head1 != NULL){
		int deleteFlag = 1;
		while(head2 != NULL){
			if(!strcmp(head1->fileInfo.filename, head2->fileInfo.filename)){
				//check if modified
				if(head2->fileInfo.timestamp != head1->fileInfo.timestamp){
					if(head2->fileInfo.type != FOLDER){
						modify_filetable_entry(fileTable,head2);
						isSame = 0;
						//printf("+++++++file %s has been modifed+++++++\n",head1->fileInfo.filename);
					}
				}
				deleteFlag = 0;
				break;
			}
			head2 = head2->next;
		}
		if(deleteFlag && mode == UPDATE){
			delete_filetable_entry(fileTable,head1);
			isSame = 0;
			//printf("+++++++file %s has been deleted+++++++\n",head1->fileInfo.filename);
		}
		head1 = head1->next;
		head2 = temp_fileTable->head;
	}
	//check if any file need add into
	head1 = fileTable->head;
	head2 = temp_fileTable->head;
	while(head2 != NULL){
		int addFlag = 1;
		while(head1 != NULL){
			if(!strcmp(head1->fileInfo.filename, head2->fileInfo.filename)){
				addFlag = 0;
				break;
			}
			head1 = head1->next;
		}
		if(addFlag){
			add_filetable_entry(fileTable,head2);
			isSame = 0;
			printf("+++++++file %s has been added+++++++\n",head2->fileInfo.filename);
		}
		head2 = head2->next;
		head1 = fileTable->head;
	}
	return isSame == 1 ? -1 : 1;
}

int update_filetable_and_peerLists (filetable_t* fileTable, filetable_t* temp_fileTable, int mode, struct in_addr peer_ip, int peer_portNum){
	//block SIGNAL
	if(fileTable->isBlocked) return -1;
	//sameFlag is initialized as TRUE
	int isSame = 1;
	//check if any file need delete
	filetable_entry_t* head1 = fileTable->head;
	filetable_entry_t* head2 = temp_fileTable->head;
	while(head1 != NULL){
		//printf("check %s\n", head1->fileInfo.filename);
		int deleteFlag = 1;
		while(head2 != NULL){
			if(!strcmp(head1->fileInfo.filename, head2->fileInfo.filename)){
				//check if modified
				if(head2->fileInfo.timestamp > head1->fileInfo.timestamp){
					if(head2->fileInfo.type == NORMALFILE){
						modify_filetable_entry(fileTable,head2);
						isSame = 0;
						//5/26: Conditions for modifying the peerlist.
						//removeAllPeersFromList(head2->peerList, head2);
						removeAllPeersFromTable(fileTable,head2->fileInfo.filename);
						addPeerToTable(peer_ip, peer_portNum, fileTable, head2->fileInfo.filename);
						//printf("+++++++file %s has been modifed+++++++\n",head1->fileInfo.filename);
					}else{
						addPeerToTable(peer_ip, peer_portNum, fileTable, head2->fileInfo.filename);
					}
				}
				else if(head2->fileInfo.timestamp == head1->fileInfo.timestamp){
					addPeerToTable(peer_ip, peer_portNum, fileTable, head2->fileInfo.filename);
				}else{
					if(head2->fileInfo.type == NORMALFILE){
						isSame = 0;
					}
                    //new added!!!!!!
                    else{
                        addPeerToTable(peer_ip, peer_portNum, fileTable, head2->fileInfo.filename);
                    }
				}
				deleteFlag = 0;
				break;
			}
			head2 = head2->next;
		}
		if(deleteFlag && mode == UPDATE){
			delete_filetable_entry(fileTable,head1);
			isSame = 0;
			//printf("+++++++file %s has been deleted+++++++\n",head1->fileInfo.filename);
		}
		else if (deleteFlag) {
			isSame = 0;
		}
		head1 = head1->next;
		head2 = temp_fileTable->head;
	}
	//check if any file need add into
	head1 = fileTable->head;
	head2 = temp_fileTable->head;
	while(head2 != NULL){
		//printf("check %s\n", head2->fileInfo.filename);
		int addFlag = 1;
		while(head1 != NULL){
			if(!strcmp(head1->fileInfo.filename, head2->fileInfo.filename)){
				addFlag = 0;
				break;
			}
			head1 = head1->next;
		}
		if(addFlag){
			add_filetable_entry(fileTable,head2);
			//5/26 fho, updateFile now modifies the filetable_entry_t's peerlist.
			//addPeerToList(peer_ip, peer_portNum, head1->peerList, head2);
			addPeerToTable(peer_ip, peer_portNum, fileTable, head2->fileInfo.filename);
			isSame = 0;
			printf("+++++++file %s has been added+++++++\n",head2->fileInfo.filename);
		}
		head2 = head2->next;
		head1 = fileTable->head;
	}
	return isSame == 1 ? -1 : 1;
}

//add file_entry to the filetable
int add_filetable_entry(filetable_t* fileTable, filetable_entry_t* entry){
	filetable_entry_t* cur = fileTable->head;
	filetable_entry_t* pre = NULL;
	char* filename = entry->fileInfo.filename;
	//malloc a filetable_entry
	filetable_entry_t* filetable_entry = (filetable_entry_t*)malloc(sizeof(filetable_entry_t));
	memset(filetable_entry,0,sizeof(filetable_entry_t));
	//initialize fileInfo
	memcpy(filetable_entry->fileInfo.filename,entry->fileInfo.filename,strlen(entry->fileInfo.filename)); //filename
	filetable_entry->fileInfo.size = entry->fileInfo.size;
	filetable_entry->fileInfo.timestamp = entry->fileInfo.timestamp;
	filetable_entry->fileInfo.type = entry->fileInfo.type;
	//find place to insert
	while(cur != NULL){
		int cmp = strcmp(filename,cur->fileInfo.filename);
		if(cmp > 0){
			pre = cur;
			cur = cur->next;
		}
		else if(cmp < 0){
			if(pre == NULL){
				//be a new header
				fileTable->head = filetable_entry;
				filetable_entry->next = cur;
				fileTable->filenumber = fileTable->filenumber + 1;
				//printf("+++++++file %s has been added+++++++\n",filename);
				return 1;
			}else{
				//insert into right position
				filetable_entry->next = cur;
				pre->next = filetable_entry;
				fileTable->filenumber = fileTable->filenumber + 1;
				//printf("+++++++file %s has been added+++++++\n",filename);
				return 1;
			}
		}else{
			printf("[add_filetable_entry]add an already existing file\n");
			return -1;
		}
	}
	if(pre == NULL){
		//fileTable is NULL
		fileTable->head = filetable_entry;
		fileTable->tail = filetable_entry;
		fileTable->filenumber = fileTable->filenumber + 1;
		//printf("+++++++file %s has been added+++++++\n",filename);
		return 1;
	}else{
		//append to the tail
		pre->next = filetable_entry;
		fileTable->tail = filetable_entry;
		fileTable->filenumber = fileTable->filenumber + 1;
		//printf("+++++++file %s has been added+++++++\n",filename);
		return 1;
	}
}

//delete a filetable entry from fileTable
int delete_filetable_entry(filetable_t* fileTable, filetable_entry_t* entry){
	filetable_entry_t* cur = fileTable->head;
	filetable_entry_t* pre = NULL;
	char* filename = entry->fileInfo.filename;
	printf("need delete %s\n",filename);
	//find place to delete
	while(cur != NULL){
		//printf("current name is %s\n",cur->fileInfo.filename);
		//printf("now cur name %s\n",cur->fileInfo.filename);
		int cmp = strcmp(filename,cur->fileInfo.filename);
		if(cmp == 0){
			if(pre == NULL){
				//delete the original head
				fileTable->head = cur->next;
				fileTable->tail = cur->next;
				fileTable->filenumber = fileTable->filenumber - 1;
				printf("+++++++file %s has been deleted+++++++\n",filename);
				free(cur);
				return 1;
			}else{
				//delete a normal node
				pre->next = cur->next;
				//if delete the tail
				if(cur->next == NULL) fileTable->tail = pre;
				fileTable->filenumber = fileTable->filenumber - 1;
				printf("+++++++file %s has been deleted+++++++\n",filename);
				free(cur);
				return 1;
			}
		}
		else{
			pre = cur;
			cur = cur->next;
		}
	}
	printf("[delete_filetable_entry]cannot find delete element\n");
	return -1;
}

//modify a filetable entry in fileTable
int modify_filetable_entry(filetable_t* fileTable, filetable_entry_t* entry){
	filetable_entry_t* cur = fileTable->head;
	char* filename = entry->fileInfo.filename;
	//find a place to modify
	while(cur != NULL){
		int cmp = strcmp(filename,cur->fileInfo.filename);
		if(cmp == 0){
			//modify fileInfo
			//memcpy(cur->fileInfo.filename,fileInfo->filename,strlen(fileInfo->filename));
			cur->fileInfo.size = entry->fileInfo.size;
			cur->fileInfo.timestamp = entry->fileInfo.timestamp;
			cur->fileInfo.type = entry->fileInfo.type;
			printf("+++++++file %s has been modifed+++++++\n",filename);
			return 1;
		}else{
			cur = cur->next;
		}
	}
	printf("[modify_filetable_entry]cannot find modify element\n");
	return -1;
}

//destroy a filetable, free all necessary memory
int destroy_filetable(filetable_t* fileTable){
	filetable_entry_t* cur = fileTable->head;
	while(cur != NULL){
		filetable_entry_t* next = cur->next;
		free(cur);
		cur = next;
	}
	return 1;
}

//get all the file information given a directory
//all filenames are full path
fileinfo_entry_t* getAllFilesInfo(const char* path, int level){
	if(level == 0){
		head = NULL;
		pre = NULL;
	}
	//printf("start scan path %s:\n",path);
	struct dirent* ent = NULL;
    DIR *pDir;
    pDir=opendir(path);
    while (NULL != (ent=readdir(pDir)))
    {
    	char end[5];
    	memcpy(end,ent->d_name+strlen(ent->d_name)-4,4);
    	end[4] = 0;
    	char start[2];
    	memcpy(start,ent->d_name,1);
    	start[1] = 0;
    	//for debugging!!!!
    	if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || !strcmp(ent->d_name,"peer") || !strcmp(end,"_tmp") || !strcmp(end,"_res") || !strcmp(start,".")) {
            continue;
        }
    	int length = strlen(path)+strlen(ent->d_name)+1;
    	char fullpath[length];
    	memset(fullpath,0,length);
    	memcpy(fullpath,path,strlen(path));
    	memcpy(fullpath+strlen(path),ent->d_name,strlen(ent->d_name));
    	//printf("++++fullpath:%s\n",fullpath);
        fileinfo_entry_t* entry = getFileInfo(fullpath,ent->d_type);
        if(pre == NULL){
        	pre = entry;
        	head = pre;
        }
        else{
        	pre->next = entry;
        	pre = entry;
        }
        if(ent->d_type == 4) {
        	char dfs[length+1];
        	memset(dfs,0,length+1);
        	memcpy(dfs,fullpath,strlen(fullpath));
    			memcpy(dfs+strlen(fullpath),"/",1);
          getAllFilesInfo(dfs,level+1); // deep first search
        }
    }
    return head;
}

//get a fileinfo of one given file
//path need be a full path
fileinfo_entry_t* getFileInfo(const char *path, int type)
{
    fileinfo_entry_t* entry = (fileinfo_entry_t*)malloc(sizeof(fileinfo_entry_t));
    memset(entry,0,sizeof(fileinfo_entry_t));
    struct stat statbuff;
    memset(&statbuff,0,sizeof(statbuff));
    if(stat(path, &statbuff) < 0){
    	printf("read state error!\n");
        return NULL;
    }else{
    	memcpy(entry->filename,path,strlen(path));
        entry->size = statbuff.st_size;
        entry->timestamp = statbuff.st_mtime;
        entry->type = type;
        return entry;
    }
}

//print FileInfo
void printFileInfo(fileinfo_entry_t* entry){
	while(entry != NULL){
		printf("filename: %s , ",entry->filename);
    	printf("filesize: %lu, ",entry->size);
    	printf("timestamp: %ld, ",entry->timestamp);
    	printf("filetype: %d.\n",entry->type);
    	entry = entry->next;
	}
    return;
}

//print FileTable
void printFileTable(filetable_t* fileTable){
	printf("*****start filetable*****\n");
	printf("File Number in the Filetable: %d\n",fileTable->filenumber);
	printf("=======================\n");
	filetable_entry_t* cur = fileTable->head;
	while(cur != NULL){
		printFileInfo(&(cur->fileInfo));
		printf("peerList has %d peers: ",cur->peerNum);
		for(int i = 0; i < cur->peerNum; i++){
			printf("%s ",inet_ntoa(cur->peerList[i].addr));
		}
		printf("\n");
		cur = cur->next;
	}
	printf("=======================\n");
	printf("*****filetable ends*****\n");
    return;
}

//if one peer dies, remove it from the peer list. should called by server
void removePeerFromTable(struct in_addr addr, filetable_t* fileTable) {
	filetable_entry_t* cur = fileTable->head;
	while(cur != NULL){
		peer_entry_t* peerList = cur->peerList;
		int err = removePeerFromList(addr,peerList,cur);
		if(err < 0) printf("remove error!!\n");
		cur = cur->next;
	}
	return;
}

//remove one peer for a file's peerlist
int removePeerFromList(struct in_addr addr, peer_entry_t* peerList, filetable_entry_t* fileEntry) {
	for(int i = 0; i < fileEntry->peerNum; i++){
		if(addr.s_addr == peerList[i].addr.s_addr){
			peerList[i] = peerList[fileEntry->peerNum-1];
			fileEntry->peerNum = fileEntry->peerNum - 1;
			return 1;
		}
	}
	return -1;
}

//removes all peers for a file's peerlist in a filetable
int removeAllPeersFromTable(filetable_t* fileTable, char* filename){
	filetable_entry_t* cur = fileTable->head;
	while(cur != NULL){
		if(!strcmp(cur->fileInfo.filename,filename)){
			removeAllPeersFromList(cur->peerList, cur);
			return 1;
		}
		cur = cur->next;
	}
	return -1;
}

//removes all peers for a file's peerlist
int removeAllPeersFromList(peer_entry_t* peerList, filetable_entry_t* fileEntry) {
	memset(peerList,0,MAX_PEER_NUMBER * sizeof(peer_entry_t));
	fileEntry->peerNum = 0;
	printf("reset peerlist for file %s\n",fileEntry->fileInfo.filename);
	return 1;
}

//add one peer to a file's peerlist
int addPeerToList(struct in_addr addr, int portNum, peer_entry_t* peerList, filetable_entry_t* fileEntry) {
	int i = 0;
	for(i = 0; i < fileEntry->peerNum; i++){
		if(addr.s_addr == peerList[i].addr.s_addr){
			return -1;
		}
	}
	peerList[i].addr = addr;
	peerList[i].portNum = portNum;
	fileEntry->peerNum = fileEntry->peerNum + 1;
	printf("add peer %s to index %d for file %s, current has %d peers\n",inet_ntoa(addr),i,fileEntry->fileInfo.filename,fileEntry->peerNum);
	return 1;
}

int addPeerToTable(struct in_addr addr, int portNum, filetable_t* fileTable, char* filename) {
	filetable_entry_t* cur = fileTable->head;
	while(cur != NULL){
		if(!strcmp(cur->fileInfo.filename,filename)){
			addPeerToList(addr,portNum,cur->peerList,cur);
			return 1;
		}
		cur = cur->next;
	}
	return -1;
}

//block monitor
void blockMonitor(filetable_t* fileTable) {
	if(fileTable->isBlocked == 1) {
		printf("allready blocked\n");
		return;
	}
	fileTable->isBlocked = 1;
	printf("blocked!!!\n");
	return;
}

//unblock monitor
void unblockMonitor(filetable_t* fileTable) {
	if(fileTable->isBlocked == 0) {
		printf("allready unblocked\n");
		return;
	}
	fileTable->isBlocked = 0;
	printf("unblocked!!!\n");
	return;
}

int initDownload(filetable_entry_t* file_entry){

    if(PTPDownloadRequestSuccessFileRes(file_entry) < 0){
        printf("ptp download res fail\n");
    }


    printf("download file %s success!!\n",file_entry->fileInfo.filename);
    return 1;
}
