#include <assert.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h>
#include <pthread.h> 
#include "mapreduce.h"

typedef struct keyNode {
    char *key;
    char **vlist;
    pthread_mutex_t keylock;
    int curtsize;
    int curused;
    int valuenum;
}keyNode;

typedef struct storeNode {
    keyNode *store;
    pthread_mutex_t storelock;
    int curtsize;
    int curused;
    int keynum;
}storeNode;

storeNode *storage;
pthread_mutex_t freelock;
pthread_mutex_t filenumlock;

int filenum = 0; //current mapping file number or reducing key number
int filetotal = -1; //argc-1 total num of file
int totalreducer; //number of reducer
Mapper mapf;
Reducer redf;
Partitioner parf;
Getter getf;

void dumpMem(){
    printf("\n\nMemory dump starting ...\n");
    for(int j = 0; j < totalreducer; j++){
        printf("Printing colum: %d\n", j);
        storeNode *store = storage + j;
        pthread_mutex_lock(&(store->storelock));
        keyNode *keyarray = store->store;
        int num = 15 < store->curused ? 15 : store->curused;
        for(int i = 0; i < num; i ++){
            keyNode* keyNode = keyarray + i;
            printf("%d : Key: %sValue:", i, keyNode->key);
            char** vlist = keyNode->vlist;
            int num2 = 15 < keyNode->curused ? 15 : keyNode->curused;
            for(int k = 0; k < num2; k ++){
                char* val = *(vlist + k);
                printf(" %s ", val);
            }
            printf("\n");
        }
        printf("\n");
	pthread_mutex_unlock(&(store->storelock));
    }
    printf("Memroy dump end ...\n\n\n");
}
/*
void freeRec(keyNode *curNode) {
    if (curNode != NULL) {
        freeRec(curNode -> next);
        free(curNode);
        curNode = NULL;
    }
}

void storagefree() {
    pthread_mutex_lock( &freelock);
	
    for (int i = 0; i < curused; i++) {
        linkNode *keyNode = storage + i;
        linkNode *curNode = keyNode -> next;
        freeRec(curNode);
        free(keyNode);
        keyNode = NULL;
    }
    free(storage);
    storage = NULL;
    pthread_mutex_unlock( &freelock);
}
*/

int compareNode(const void*node1, const void*node2){
    char *key1 = ((keyNode*)node1) -> key;
    char *key2 = ((keyNode*)node2) -> key;
    return strcmp(key1, key2);
}

int compareValue(const void*value1, const void*value2){
    return strcmp((char*)value1, (char*)value2);
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash *33 + c;
    return hash % num_partitions;
}

void MR_Emit(char *key, char *value) {
    int partition = parf(key, totalreducer);
    storeNode* store = storage + partition;
    pthread_mutex_lock(&(store->storelock));
    keyNode* keyarray = store->store;
    if (store->curtsize == store->curused) {
        keyarray = (keyNode*)realloc(keyarray, (store->curtsize * sizeof(keyNode)) << 1);
        store->curtsize *= 2;
        memset((keyarray + store->curused), 0,  (store->curtsize - store->curused)*sizeof(keyNode));
        store -> store = keyarray;
    }
    //linear search for key position
    for(int i = 0; i < store->curused; i++) {
        keyNode *keyNode = keyarray + i;
		if(strcmp(keyNode->key, key) == 0) {
            if(keyNode->curtsize == keyNode->curused){ //TODO modify locks
                keyNode->vlist = (char**)realloc(keyNode->vlist, (keyNode->curtsize * sizeof(char*)) << 1);
                keyNode->curtsize *=2;
                memset((keyNode->vlist + keyNode->curused), 0, (keyNode->curtsize - keyNode->curused) * sizeof(char*));
			}
            char** vlist = keyNode->vlist;
            char** val = vlist + keyNode->curused;
            keyNode->curused ++;
            *val = malloc(strlen(value) + 1);
            strcpy(*val, value);
            
			pthread_mutex_unlock(&(store->storelock));
			return;
		}
    }
    keyNode newKey;
    newKey.key = calloc(1, strlen(key) + 1);
    strcpy(newKey.key, key);
    if (pthread_mutex_init( &(newKey.keylock), NULL) != 0) {
        printf("Error: failed to init entrylock \n");
        exit(1);
    }
    newKey.curtsize = 10;
    newKey.vlist = calloc(1, 10*sizeof(char*));
    newKey.curused = 1;
    newKey.valuenum = 0;
    *(newKey.vlist) = malloc(strlen(value) + 1);
    strcpy(*(newKey.vlist), value);
    //pthread_mutex_lock(&(store->storelock));
    memcpy((store->store + store->curused), &newKey, sizeof(keyNode));
    store->curused++;
    pthread_mutex_unlock( &(store->storelock));
    //dumpMem();
}

char* get_next(void *key, int partition_number){
    keyNode tempkey;
    tempkey.key = (char*)key;
    storeNode* store = (storage + partition_number);
    keyNode* keyarray = store -> store;
    void* keyptr = bsearch((void*)(&tempkey), keyarray, store->curused, sizeof(keyNode), compareNode);
    keyNode* curkey = (keyNode*)keyptr;
    pthread_mutex_lock(&(curkey->keylock));
    char** vlist = curkey->vlist;
    //pthread_mutex_lock(&(curkey->keylock));
    if(curkey->valuenum >= curkey->curused){
        pthread_mutex_unlock(&(curkey->keylock));
        return NULL;
    }
    char* val = *(vlist + curkey->valuenum);
	//printf("|%d|\n", curkey->valuenum);
    curkey->valuenum ++;
    pthread_mutex_unlock(&(curkey->keylock));
	//printf("val: %s\n", val);
    return val;
}

void* mapstart(void *argv) {
    if (filetotal < 0) {
        printf("Error: Filetatal is < 0\n");
        exit(1);
    }
    while(1) {
        pthread_mutex_lock(&filenumlock);
        if (filenum >= filetotal) {
            pthread_mutex_unlock(&filenumlock);
            return NULL;
        }
        char *curfile = *((char**)argv + (filenum++));
        pthread_mutex_unlock(&filenumlock);
        (*mapf)(curfile);
    }
}

void* reducestart(void *argv) {
    int partition = *((int*)argv);
    storeNode* store = storage + partition;
	
    keyNode* keyarray = store->store;
	while (1){
		pthread_mutex_lock(&store->storelock);
		if(store->keynum >= store->curused){
			return NULL;
		}

		char* key = (keyarray + (store->keynum)) -> key;
		store->keynum ++;
		pthread_mutex_unlock(&store->storelock);
		(*redf)(key, getf, partition);
	}
}

void sortStorage(){
    for(int d = 0; d < totalreducer; d++){
        storeNode* store = (storage + d);
        pthread_mutex_lock(&(store->storelock));
        keyNode* keyarray = store -> store;
        qsort(keyarray, store->curused, sizeof(keyNode), compareNode);
		for(int e = 0; e < store->curused; e++){
            keyNode* curkey = keyarray + e;
            qsort(curkey->vlist, curkey->curused, sizeof(char*), compareValue);
		}
		pthread_mutex_unlock(&(store->storelock));
    }
}//TODO

void MR_Run(int argc, char *argv[],
    Mapper map, int num_mappers,
    Reducer reduce, int num_reducers,
    Partitioner partition) {
    if (pthread_mutex_init( &freelock, NULL) != 0) {
        printf("Error: failed to init freelock \n");
        exit(1);
    }
    pthread_mutex_init(&filenumlock,NULL);
    filetotal = argc - 1;
	
    totalreducer = num_reducers;
    storage = calloc(totalreducer, sizeof(storeNode));
    if (storage == NULL) {
        printf("Error: failed to allocate for storage\n");
        exit(1);
    }
    for(int a = 0; a < totalreducer; a++){
        storeNode* store = storage + a;
        store -> store = calloc(10, sizeof(keyNode));
        store -> curtsize = 10;
        pthread_mutex_init(&(store->storelock), NULL);
	keyNode* keyarray = store -> store;
	for(int f = 0; f < 10; f++){
            keyarray -> curtsize = 10;
            keyarray -> vlist = calloc(10, sizeof(char*));
	    pthread_mutex_init(&(keyarray->keylock), NULL);
	}
    }

    if (partition == NULL) parf = MR_DefaultHashPartition;
    else parf = partition;
    mapf = map;
    redf = reduce;
    getf = (Getter)get_next;

    int minthread = (num_mappers < filetotal) ? num_mappers : filetotal;
    pthread_t mthreads[minthread];
    filenum = 0;
    for (int i = 0; i < minthread; i++) {
        if (pthread_create(&mthreads[i], NULL, mapstart, argv + 1) != 0) {
            printf("Error: Failed to create mapper thread: %di\n", i);
            exit(1);
        }
    }
    for (int k = 0; k < minthread; k++) {
        pthread_join(mthreads[k], NULL);
    }
    
    sortStorage();
    //dumpMem();
    pthread_t rthreads[totalreducer];
    for (int j = 0; j < totalreducer; j++) {
        int* rargv = malloc(sizeof(int));
        *rargv = j;
        if (pthread_create(&rthreads[j], NULL, reducestart, rargv) != 0) {
            printf("Error: Failed to create reducer threadL %d\n", j);
            exit(1);
        }
		//free(rargv); rargv = NULL;
    }
    for (int l = 0; l < totalreducer; l++) {
        pthread_join(rthreads[l], NULL);
    }
    //dumpMem();
}
