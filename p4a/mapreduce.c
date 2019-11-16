#include <assert.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <pthread.h> 
#include "mapreduce.h"

typedef struct keyNode {
    char *key;
    char *val;
    pthread_mutex_t keylock;
}keyNode;

typedef struct storeNode {
    keyNode *store;
    pthread_mutex_t storelock;
    int curtsize;
    int curused;
    keyNode *getkey;
    keyNode *lastkey;
}storeNode;

storeNode *storage;
pthread_mutex_t filenumlock;

int filenum = 0; //current mapping file number or reducing key number
int filetotal = -1; //argc-1 total num of file
int totalreducer; //number of reducer
Mapper mapf;
Reducer redf;
Partitioner parf;
Getter getf;
/*
void dumpMem(){
    printf("\n\nMemory dump starting ...\n");
    for(int j = 0; j < totalreducer; j++){
        printf("Printing colum: %d\n", j);
        storeNode *store = storage + j;
        keyNode *keyarray = store->store;
	int n = 100;
        int num = n < store->curused ? n : store->curused;
        for(int i = 0; i < num; i ++){
            keyNode* keyNode = keyarray + i;
            printf("%d : Key: %s", i, keyNode->key);
            printf("Value: %s\n", keyNode->val);
        }
        printf("\n");
    }
    printf("Memroy dump end ...\n\n\n");
}
*/
void storagefree() {
    for (int i = 0; i < totalreducer; i++) {
        storeNode *store = storage + i;
        keyNode *keyarray = store->store;
	for(int j = 0; j < store->curtsize; j++){
            keyNode* keyNode = keyarray + j;
	    free(keyNode->key);
	    free(keyNode->val);
	    pthread_mutex_destroy(&(keyNode->keylock));
	}
	pthread_mutex_destroy(&(store->storelock));
	free(store->store);
	free(store->lastkey);
    }
    pthread_mutex_destroy(&filenumlock);
    free(storage);
    storage = NULL;
}

int compareNode(const void*node1, const void*node2){
    char *key1 = ((keyNode*)node1) -> key;
    char *key2 = ((keyNode*)node2) -> key;
    char *val1 = ((keyNode*)node1) -> val;
    char *val2 = ((keyNode*)node2) -> val;
    int i = strcmp(key1, key2);
    if(i == 0) i = strcmp(val1, val2);
    return i;
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
    // reallocate
    if (store->curtsize == store->curused) {
        keyarray = (keyNode*)realloc(keyarray, 2 * store->curtsize * sizeof(keyNode));
	store->curtsize *= 2;
        memset((keyarray + store->curused), 0,  (store->curtsize - store->curused)*sizeof(keyNode));
        store -> store = keyarray;
    }
    // add new pairs
    keyNode newKey;
    newKey.key = calloc(1, strlen(key));
    newKey.val = calloc(1, strlen(value));
    strcpy(newKey.key, key);
    strcpy(newKey.val, value);
    if (pthread_mutex_init( &(newKey.keylock), NULL) != 0) {
        printf("Error: failed to init entrylock \n");
        exit(1);
    }
    memcpy((store->store + store->curused), &newKey, sizeof(keyNode));
    store->curused++;
    pthread_mutex_unlock( &(store->storelock));
}

char* get_next(void *key, int partition_number){
    storeNode* store = storage + partition_number;
    keyNode* curkey = store -> getkey;
    pthread_mutex_lock(&(curkey->keylock));
    // quit if not current key
    if(curkey->key == NULL || strcmp(store->lastkey->key, curkey->key) != 0){
        pthread_mutex_unlock(&(curkey->keylock));
        return NULL;
    }
    char* val = curkey -> val;
    store->getkey = (keyNode*)((void*)store->getkey + sizeof(keyNode));
    pthread_mutex_unlock(&(curkey->keylock));
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
    store -> getkey = store->store;
    char* newkey;
    keyNode* end = store->store + store->curused;
    store->lastkey = malloc(sizeof(keyNode));
    store->lastkey->key = NULL;
    while(store->getkey < end){
        newkey = store->getkey->key;
        if(store->lastkey->key == NULL || strcmp(store->lastkey->key, newkey) != 0){
            store->lastkey->key = newkey;
            (*redf)(newkey, getf, partition);
        }
    }
    return NULL;
}

void sortStorage(){
    for(int d = 0; d < totalreducer; d++){
        storeNode* store = (storage + d);
        qsort(store->store, store->curused, sizeof(keyNode), compareNode);
    }
}

void MR_Run(int argc, char *argv[],
    Mapper map, int num_mappers,
    Reducer reduce, int num_reducers,
    Partitioner partition) {
    
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
    pthread_t rthreads[totalreducer];
    int* rargv[totalreducer];
    for (int j = 0; j < totalreducer; j++) {
	rargv[j] = malloc(sizeof(int));
        *(rargv[j]) = j;
        if (pthread_create(&rthreads[j], NULL, reducestart, rargv[j]) != 0) {
            printf("Error: Failed to create reducer threadL %d\n", j);
            exit(1);
        }
    }
    for (int l = 0; l < totalreducer; l++) {
        pthread_join(rthreads[l], NULL);
    }
    for(int m = 0; m < totalreducer; m++){
	free(rargv[m]); rargv[m] = NULL;
    }
    storagefree();
}
