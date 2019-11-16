#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define ROOTINO 1
#define DIRSIZ 14
#define BSIZE 512
// Inodes per block.
#define IPB (BSIZE / sizeof(dinode))

typedef struct super_block{
    uint size;
    uint nblocks;
    uint ninodes;
} super_block;

typedef struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
} dinode;

typedef struct dirent {
  ushort inum;
  char name[DIRSIZ];
} dirent;

int img;
void *img_ptr;
void finish(){
    if(img != -1){
        close(img);
    }
}
void check_parent_recur(int node_num);
void check_parent_block(int block_index, int node_num);
int search_child_block(dirent* block, int node_num);
int search_child(dinode* child_node, int node_num);
void check_parent();
void findIndex(int i, dirent** rootdir, dirent** parentdir);

void bad_inode(){
    fprintf(stderr, "ERROR: bad inode.\n");
    finish();
    exit(1);
}

void bad_direct_addr(){
    fprintf(stderr, "ERROR: bad direct address in inode.\n");
    finish();
    exit(1);
}

void bad_indirect_addr(){
    fprintf(stderr, "ERROR: bad indirect address in inode.\n");
    finish();
    exit(1);
}

void bad_root_dir(){
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    finish();
    exit(1);
}

void bad_dir_format(){
    fprintf(stderr, "ERROR: directory not properly formatted.\n");
    finish();
    exit(1);
}

void addr_inused_marked_free(){
    fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
    finish();
    exit(1);
}

void addr_freed_marked_inuse(){
    fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
    finish();
    exit(1);
}

void dup_direct_addr(){
    fprintf(stderr, "ERROR: direct address used more than once.\n");
    finish();
    exit(1);
}

void dup_indirect_addr(){
    fprintf(stderr, "ERROR: indirect address used more than once.\n");
    finish();
    exit(1);
}

void inode_marked_inuse_notfound(){
    fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
    finish();
    exit(1);
}

void inode_point_free_dir(){
    fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
    finish();
    exit(1);
}

void file_bad_reference_count(){
    fprintf(stderr, "ERROR: bad reference count for file.\n");
    finish();
    exit(1);
}

void dup_dir(){
    fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
    finish();
    exit(1);
}
////////////
void parent_dir_mismatch(){
    fprintf(stderr, "ERROR: parent directory mismatch.\n");
    finish();
    exit(1);
}

void bad_dir(){
    fprintf(stderr, "ERROR: inaccessible directory exists.\n");
    finish();
    exit(1);
}

void print_node(dinode dip){
    printf("file type:%d,", dip.type);
    printf("nlink:%d,", dip.nlink);
    printf("size:%d,", dip.size);
    printf("first_addr:%d\n", dip.addrs[0]);
}

/////////////
super_block *sb;
dinode *start;
char* bitmap;
uint sbitmap_bytes;
uint bitmap_block;
int dircount;
int rflag;
dinode* lost_found;
void* free_data_block;

void find_lf(){
    dinode* root = &start[ROOTINO];
    int j;
    for(j = 0 ; j < NDIRECT; j++){
        if(root->addrs[j]){
            dirent* cur_dirent = img_ptr + (BSIZE*(root -> addrs[j]));
            for(int b = 0; b < BSIZE/sizeof(dirent); b++){
                if(!cur_dirent->inum){
                    cur_dirent++;
                    continue;
                }
                if(strcmp(cur_dirent -> name, "lost_found") == 0){
                    lost_found = &start[cur_dirent -> inum];
                }
                cur_dirent ++;
            }
        }
    }
    if(root -> addrs[NDIRECT]){
        uint* dir_entry = (uint*)(img_ptr + (root -> addrs[NDIRECT]) * BSIZE);
        for(int j = 0; j < NINDIRECT; j++){
            if(dir_entry[j]){
                dirent* cur_dirent = (dirent*)(img_ptr + (BSIZE*(dir_entry[j])));
                for(int c = 0; c < BSIZE/sizeof(dirent); c++){
                    if(!(cur_dirent -> inum)){
                        cur_dirent++;
                        continue;
                    }
                    if(strcmp(cur_dirent -> name, "lost_found") == 0){
                        lost_found = &start[cur_dirent -> inum];
                    }
                    cur_dirent++;
                }
            }
        }
    }
}

void find_free_block(){
    for(int i = 0; i < sb -> nblocks; i++){
        // printf("block total %d %d uint fdblock addr %p\n", sb->nblocks, i, free_data_block);
        if(!((dirent*)free_data_block)->inum){
            return;
        }
        free_data_block += BSIZE;
    }
}

void parse_file(){
    struct stat sbuf;
    fstat(img, &sbuf);
    if(rflag){
        if((img_ptr = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, img, 0)) < 0){
            fprintf(stderr, "Failed to mmap\n");
            exit(1);
        }
    }else{
        if((img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, img, 0)) < 0){
            fprintf(stderr, "Failed to mmap\n");
            exit(1);
        }
    }
    
    sb = (super_block*)(img_ptr + BSIZE);
    start = (dinode*)(img_ptr + 2 * BSIZE);
    bitmap_block = sb->ninodes / IPB + 3;
    bitmap = (char*) (img_ptr + (BSIZE * bitmap_block));
    sbitmap_bytes = 1024/8;
    dircount = 0;
    free_data_block = img_ptr + (BSIZE * (bitmap_block + 1));
    if(rflag){
        find_lf();
        find_free_block();
    }
}

void repair_img(uint inum){
    // search for lost_found
    int i, j, saved;
    dirent* temp;
    // Direct
    for(j = 0; j < NDIRECT; j++){
        int addr = lost_found -> addrs[j];
        // have space
        if(addr){
            temp = img_ptr + BSIZE * addr;
            
            for(i = 0; i < BSIZE/sizeof(dirent); i++){
                if(!temp[i].inum){
                    temp[i].inum = inum;    
                    strcpy(temp[i].name, "lost_");
                    char str[7];
                    sprintf(str, "%d", inum);
                    strcat(temp[i].name, str);
                    saved++;
                    j = NDIRECT;
                    // change parent ref
                    if(start[inum].type == T_DIR){
                        dirent* dot = NULL;
                        dirent* dotdot = NULL;
                        findIndex(inum, &dot, &dotdot);                
                        dotdot -> inum = (uint)(lost_found - start);
                    }
                    break;
                }
            }
        }
        // no space new block
        else {
            temp = (dirent*)free_data_block;
            lost_found->addrs[j] = (free_data_block - img_ptr)/BSIZE;
            for(i = 0; i < BSIZE/sizeof(dirent); i++){
                if(!temp[i].inum){
                    temp[i].inum = inum;
                    strcpy(temp[i].name, "lost_");
                    char str[7];
                    sprintf(str, "%d", inum);
                    strcat(temp[i].name, str);
                    saved++;
                    j = NDIRECT;
                    // change parent ref
                    if(start[inum].type == T_DIR){
                        dirent* dot;
                        dirent* dotdot;
                        findIndex(inum, &dot, &dotdot);
                        dotdot -> inum = (uint)(lost_found - start);
                    }
                    break;
                }
            }
            find_free_block();
        }
    }
    
    // Indirect
    if(!saved){
        // setup if not exist
        if(!(lost_found -> addrs[NDIRECT])){
            void* temp = free_data_block;
            lost_found -> addrs[NDIRECT] = (temp - img_ptr)/BSIZE;
            find_free_block();
        }
        uint* dir_entry = (uint*)(img_ptr + (lost_found -> addrs[NDIRECT]) * BSIZE);
        for(int j = 0; j < NINDIRECT; j++){
            int addr = dir_entry[j];
            // have space
            if(addr){
                temp = img_ptr + BSIZE * addr;
                for(i = 0; i < BSIZE/sizeof(dirent); i++){
                    if(!temp[i].inum){
                        temp[i].inum = inum;
                        strcpy(temp[i].name, "lost_");
                        char str[7];
                        sprintf(str, "%d", inum);
                        strcat(temp[i].name, str);
                        saved++;
                        j = NINDIRECT;
                        // change parent ref
                        if(start[inum].type == T_DIR){
                            dirent* dot;
                            dirent* dotdot;
                            findIndex(inum, &dot, &dotdot);
                            dotdot -> inum = (uint)(lost_found - start);
                        }
                        break;
                    }
                }
            }
            // no space new block
            else {
                temp = (dirent*)free_data_block;
                dir_entry[j] = (free_data_block - img_ptr)/BSIZE;
                for(i = 0; i < BSIZE/sizeof(dirent); i++){
                    if(!temp[i].inum){
                        temp[i].inum = inum;
                        strcpy(temp[i].name, "lost_");
                        char str[7];
                        sprintf(str, "%d", inum);
                        strcat(temp[i].name, str);
                        saved++;
                        j = NINDIRECT;
                        // change parent ref
                        if(start[inum].type == T_DIR){
                            dirent* dot;
                            dirent* dotdot;
                            findIndex(inum, &dot, &dotdot);
                            dotdot -> inum = (uint)(lost_found - start);
                        }
                        break;
                    }
                }
                find_free_block();
            }
        }
        if(!saved){
            // printf("Lost Found is Full!\n");
            // exit(1);
        }
    }
}

/*
    21 <== 20  <== 22
    |               ^
    ----------------
*/


void printall(){
    int j;
    int count;
    printf("\n\n\n\n----------------------------------start--------------------------------------\n");
    for(int i = 0; i < sb -> ninodes; i++){
        dinode* node = &start[i];
        if(node->type) {
            count ++;
            printf("\nStarting Inode: %d \n", i);
            //print_node(*node);
            if(node->type == T_DIR) {
                printf("----------------------------------direct--------------------------------------\n");
                for(j = 0; j < NDIRECT; j++){
                    if((node->addrs[j])){
                        printf("Starting block: j = %d address: %d \n", j, node->addrs[j]);
                        dirent* cur_dirent = img_ptr + (BSIZE*(node -> addrs[j]));
                        for(int b = 0; b < BSIZE/sizeof(dirent); b++){
                            // if(!(cur_dirent->inum)) continue;
                            printf("inum: |%d| Name: |%s| i:|%ld|\n", cur_dirent -> inum, cur_dirent -> name, j * BSIZE/sizeof(dirent) + b);
                            cur_dirent++;
                        }
                    }
                }
                if((node->addrs[NDIRECT])) {
                    printf("----------------------------------indirect--------------------------------------\n");
                    uint* dir_entry = (uint*)(img_ptr + (node -> addrs[NDIRECT]) * BSIZE);
                    for(j = 0; j < NINDIRECT; j++){
                        if((dir_entry[j])) {
                            printf("Starting block: j = %d address: %d \n", j, BSIZE*dir_entry[j]);
                            dirent* cur_dirent = (dirent*)(img_ptr + (BSIZE*(dir_entry[j])));
                            for(int c = 0; c < BSIZE/sizeof(dirent); c++){
                                if((cur_dirent->inum)) {
                                    printf("inum: |%d| Name: |%s|\n", cur_dirent -> inum, cur_dirent -> name);
                                    cur_dirent++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    printf("Number of inode: %d\n", count);
    printf("----------------------------------finished--------------------------------------\n");
}

void findIndex(int i, dirent** rootdir, dirent** parentdir){
    
    dinode* temp = &start[i];
    short temp_type = temp -> type;
    // Check root dir
    int root_count = 0;
    int parent_count = 0;
    if(temp_type == T_DIR){
        for(int k = 0; k < NDIRECT; k++){
            dirent* cur_dirent = img_ptr + (BSIZE*(temp -> addrs[k]));
            for(int b = 0; b < BSIZE/sizeof(dirent); b++){
                if(!(cur_dirent -> inum)){
                    cur_dirent++;   
                    continue;
                }
                if(strcmp(".", cur_dirent -> name) == 0){
                    if(root_count){
                        printf("Error: more than one . \n");
                    }
                    root_count++;
                    *rootdir = cur_dirent;
                }
                else if(strcmp("..", cur_dirent -> name) == 0){
                    if(parent_count){
                        printf("Error: more than one .. \n");
                    }
                    parent_count++;
                    *parentdir = cur_dirent;
                }
                cur_dirent++;
            }
        }
        
        if(temp -> addrs[NDIRECT]){
            uint* dir_entry = (uint*)(img_ptr + (temp -> addrs[NDIRECT]) * BSIZE);
            for(int j = 0; j < NINDIRECT; j++){
                dirent* cur_dirent = (dirent*)(img_ptr + (BSIZE*(dir_entry[j])));
                for(int c = 0; c < BSIZE/sizeof(dirent); c++){
                    if(!(cur_dirent -> inum)){
                        cur_dirent++;
                        continue;
                    }
                    if(strcmp(".", cur_dirent -> name) == 0){
                        if(root_count){
                            printf("Error: more than one . \n");
                        }
                        root_count++;
                        *rootdir = cur_dirent;
                    }
                    else if(strcmp("..", cur_dirent -> name) == 0){
                        if(parent_count){
                            printf("Error: more than one .. \n");
                        }
                        parent_count++;
                        *parentdir = cur_dirent;
                    }
                    cur_dirent++;
                }
            }
        }
        if(root_count != 1 || parent_count != 1){
            bad_dir_format();
        }
    }
}

void check_inodes(){
    dinode *temp = start;
    dirent* rootdir;
    dirent* parentdir;
    for(int i = 0; i < sb -> ninodes; i++){
        short temp_type = temp -> type;
        /** first check**/
        if(temp_type < 0 || temp_type > T_DEV){
            bad_inode();
        }
        // Check root dir
        findIndex(i, &rootdir, &parentdir);
        
        if(i == ROOTINO && temp_type != T_DIR){
            bad_root_dir();
        }
        else if(i == ROOTINO && temp_type == T_DIR){
            if (rootdir -> inum != parentdir -> inum  && (rootdir -> inum == ROOTINO || parentdir -> inum == ROOTINO)){ 
                bad_root_dir();
            }
        }
        // Check dir addr
        int j;
        if(temp_type != 0){
            for(j = 0 ; j < NDIRECT; j++){
                if(temp -> addrs[j] < 0 || temp -> addrs[j] > 1023){
                    bad_direct_addr();
                }
            }
            uint* entry = (uint*)(img_ptr + (temp -> addrs[NDIRECT]) * BSIZE);
            for(j = 0; j < NINDIRECT; j++){
                if(entry[j] < 0 || entry[j] > 1023){
                    bad_indirect_addr();
                }
            }
        }
        // Check dir format
        if(temp_type == T_DIR){
            if(rootdir->inum != i || strcmp(rootdir->name, ".") != 0 || strcmp(parentdir->name, "..") != 0) {
                bad_dir_format();
            }
        }
        temp ++;
    }
}

void count_inodes_addr(char* storage){
    dinode *temp = start;
    char indirect_storage[1024] = {};
    int i;
    for(i = 0; i < sb -> ninodes; i++){
        short temp_type = temp -> type;
        if(temp_type){
            int l;
            for(l = 0; l < NDIRECT + 1; l++){
                uint bindex = temp->addrs[l];
                if(storage[bindex]){
                    dup_direct_addr();
                }
                if(bindex){
                    storage[bindex] ++;
                    if(l == NDIRECT){
                        if(indirect_storage[bindex]){
                            dup_indirect_addr();
                        }
                        indirect_storage[bindex] ++;
                    }
                }
            }
            if(temp->addrs[NDIRECT] > 0){
                uint* entry = (uint*)(img_ptr + (temp->addrs[NDIRECT]) * BSIZE);
                for(l = 0; l < NINDIRECT; l ++){
                    uint bindex = entry[l];
                    if(storage[bindex]){
                        dup_direct_addr();
                    }
                    if(bindex){
                        if(indirect_storage[bindex]){
                            dup_indirect_addr();
                        }
                        indirect_storage[bindex] ++;
                        storage[bindex] ++;
                    }
                }
            }
        }
        temp ++;
    }
    for(i = 0; i < bitmap_block + 1; i ++){
        storage[i]++;
    }
}

void check_bitmap(){
    dinode *temp = start;
    // test inused free
    int i, j;
    for(i = 0; i < sb -> ninodes; i++){
        short temp_type = temp -> type;
        if(temp_type){
            // Check direct bitmap
            for(j = 0; j < NDIRECT; j++){
                uint bindex = temp->addrs[j];
                uint bitmask = 0x1 << (bindex%8);
                uint mark = bitmap[bindex/8] & bitmask;
                if(bindex && !mark){
                    addr_inused_marked_free();
                }
            }
            // Check indirect bitmap
            if(temp->addrs[NDIRECT] > 0){
                uint* entry = (uint*)(img_ptr + (temp->addrs[NDIRECT]) * BSIZE);
                for(j = 0; j < NINDIRECT; j ++){
                    uint bindex = entry[j];
                    uint bitmask = 0x1 << (bindex%8);
                    uint mark = bitmap[bindex/8] & bitmask;
                    if(bindex && !mark){
                        addr_inused_marked_free();
                    }
                }
            }
        }
        temp ++;
    }
    char storage[1024] = {};
    count_inodes_addr(storage);
    for(i = 0; i < sbitmap_bytes; i ++){
        uint byte = bitmap[i];
        for(j = 0; j < 8; j ++){
            uint bitmask = 0x1 << j;
            uint mark = byte & bitmask;
            if(mark){
                int index = i*8 + j;
                int count = storage[index];
                if(!count) {
                    addr_freed_marked_inuse();
                }
            }
        }
    }
}

void check_data(){
    dinode *temp;
    int* ref_inode;
    int* ref_dirent;
    int* ref_link;
    int* ref_dir;
    int i;
    int restarted = 0;
restart:
    temp = start;
    if((ref_inode = (int*)calloc(sb->ninodes, sizeof(int))) == NULL){
        printf("ERROR: calloc() failed.\n");
        finish();
        exit(1);
    }
    if((ref_dirent = (int*)calloc(sb->ninodes, sizeof(int))) == NULL){
        printf("ERROR: calloc() failed.\n");
        finish();
        exit(1);
    }
    if((ref_link = (int*)calloc(sb->ninodes, sizeof(int))) == NULL){
        printf("ERROR: calloc() failed.\n");
        finish();
        exit(1);
    }
    if((ref_dir = (int*)calloc(sb->ninodes, sizeof(int))) == NULL){
        printf("ERROR: calloc() failed.\n");
        finish();
        exit(1);
    }
    // check inuse
    for(i = 0; i < sb -> ninodes; i++){
        if(temp -> type){
            ref_inode[i] = 1;
            dircount += temp->nlink;
        }
        temp++;
    }
    temp = start;
    // loop inodes
    for(i = 0; i < sb -> ninodes; i++){
        if(temp -> type == T_DIR){
            // dir
            // loop data block
            int j;
            for(j = 0; j < NDIRECT; j ++){
                dirent* data_entry = (dirent*)(img_ptr + BSIZE*(temp->addrs[j]));
                // loop dir ent
                for(int k = 0; k < BSIZE/sizeof(dirent); k++){
                    if(data_entry->inum && data_entry->inum != i){
                        ref_dirent[data_entry->inum] ++;
                        if(start[data_entry->inum].type == T_FILE){
                            ref_link[data_entry->inum] ++;
                        }
                        else if(start[data_entry->inum].type == T_DIR){
                            if (strcmp(data_entry->name, ".") != 0 && strcmp(data_entry->name, "..") != 0){
                                ref_dir[data_entry->inum]++;
                            }
                        }
                    }
                    data_entry ++;
                }
            }
            // indir
            uint* entry = (uint*)(img_ptr + (temp -> addrs[NDIRECT]) * BSIZE);
            for(j = 0; j < NINDIRECT; j++){
                dirent* data_entry = (dirent*)(img_ptr + BSIZE*(entry[j]));
                for(int k = 0; k < BSIZE/sizeof(dirent); k++){
                    if(data_entry->inum && data_entry->inum != i){
                        ref_dirent[data_entry->inum] ++;
                        if(start[data_entry->inum].type == T_FILE){
                            ref_link[data_entry->inum] ++;
                        }
                        else if(start[data_entry->inum].type == T_DIR){
                            if (strcmp(data_entry->name, ".") != 0 && strcmp(data_entry->name, "..") != 0){
                                ref_dir[data_entry->inum]++;
                            }
                        }
                    }
                    data_entry ++;
                }
            }
        }
        temp ++;
    }
    // check_parent();
    ref_dirent[1] ++;
    for(i = 0; i < sb -> ninodes; i++){
        if(!ref_dirent[i] && ref_inode[i]){
            if(rflag){
                repair_img(i);
            }
            else{
                inode_marked_inuse_notfound();
            }
        }
    }
    if(rflag && !restarted){
        free(ref_inode);
        free(ref_dirent);
        free(ref_link);
        free(ref_dir);
        restarted ++;
        dircount = 0;
        goto restart;
    }

    ///save file;
    for(i = 0; i < sb -> ninodes; i++){
        if(ref_dirent[i] && !ref_inode[i]){
            inode_point_free_dir();
        }
        if(ref_link[i] != start[i].nlink && start[i].type == T_FILE){
            file_bad_reference_count();
        }
        if(ref_dir[i] > 1){
            dup_dir();
        }
    }
    free(ref_inode);
    free(ref_dirent);
    free(ref_link);
    free(ref_dir);
}

int search_child_block(dirent* block, int node_num){
    for(int j = 0; j < BSIZE/sizeof(dirent); j++){
        if(!(block -> inum)){
            block++;
            continue;
        }
        //printf("Printing dirent = %d | name = %s \n", j, block->name);
        if(strcmp(block->name, "..") == 0){
            //printf("get .. from dirent = %d | for node_num = %d | block inum %d \n", j, node_num, block->inum);
            if(block->inum == node_num){
                return 1;
            }
        }
        block++;
    }
    return 0;
}

int search_child(dinode* child_node, int node_num){
    // printf("searching dir of child, node_num = %d \n", node_num);
    int i;
    for(i = 0; i < NDIRECT; i++){
        dirent* cur_block = (dirent*)(img_ptr + BSIZE * child_node->addrs[i]);
        if(search_child_block(cur_block, node_num)){
            return 1;
        }
    }
    uint* entry = (uint*)(img_ptr + (child_node -> addrs[NDIRECT]) * BSIZE);
    for(i = 0; i < NINDIRECT; i++){
        dirent* cur_block = (dirent*)(img_ptr + BSIZE * entry[i]);
        if(search_child_block(cur_block, node_num)){
            return 1;
        }
    }
    return 0;
}

void check_parent_block(int block_index, int node_num){
    dirent* curr_dir = (dirent*)(img_ptr + BSIZE * block_index);
    for(int j = 0; j < BSIZE/sizeof(dirent); j++){
        if(!(curr_dir -> inum)){
            curr_dir++;
            continue;
        }
        int child_num = curr_dir -> inum;
        char* child_name = curr_dir -> name;
        if(child_num && strcmp(child_name, ".") && strcmp(child_name, "..")){
            dinode* child_node = &start[child_num];
            if(child_node -> type){
                dircount--;
            }
            //printf("Child name = %s | child num %d | child type %d | addrs index %d\n", child_name, child_num, child_node->type, j);
            if(child_node -> type == T_DIR){
                if(!search_child(child_node, node_num)){
                    parent_dir_mismatch();
                }
                check_parent_recur(child_num);
            }
        }
        curr_dir++;
    }
}

void check_parent_recur(int node_num){
    dinode* node = &(start[node_num]);
    //printf("\n\nStart of recur!!!!!!\n");
    int i;
    if(node -> type == T_DIR){
        for(i = 0; i < NDIRECT; i++){
            int block_index = node -> addrs[i];
            check_parent_block(block_index, node_num);
        }
        uint* entry = (uint*)(img_ptr + (node->addrs[NDIRECT]) * BSIZE);
        for(i = 0; i < NINDIRECT; i++){
            int block_index = entry[i];
            check_parent_block(block_index, node_num);
        }
    }
    //printf("End of recur!!!!!!\n\n\n");
}

void check_parent(){
    //printf("Before: |%d|\n", dircount);
    dircount -= start[ROOTINO].nlink;
    check_parent_recur(ROOTINO);
    //printf("After: |%d|\n", dircount);
    if(dircount){
        bad_dir();
    }
}

int check_img(){
    parse_file();
    check_inodes();
    check_bitmap();
    check_data();
    check_parent();
    //printall();
    return 0;
}

int main(int argc, char* argv[]){
    if(argc != 2 && argc != 3){
        fprintf(stderr, "Usage: xcheck <file_system_image>\n");
        exit(1);
    }
    else{
        char* image_name = argv[1];
        rflag = 0;
        if(argc == 3){
            rflag = 1;
            if(strcmp(argv[1], "-r") == 0){
                image_name = argv[2];
            }
            else {
                fprintf(stderr, "Usage: xcheck -r <file_system_image>\n");
                exit(1);
            }
        }
        if(rflag){
            if((img = open(image_name, O_RDWR)) < 0){
                fprintf(stderr, "image not found.\n");
                exit(1);
            }
        }else{
            if((img = open(image_name, O_RDONLY)) < 0){
                fprintf(stderr, "image not found.\n");
                exit(1);
            }
        }
        check_img();
        finish();
    }
    exit(0);
}