#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <math.h>

char error_message[30] = "An error has occurred\n";
void error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}
char **pathspace;
int pathcount = 0;
char **history;
int historycount = 0;
int totalhsize = 0;
const int hpagesize = 10;
// grow the history array by 10 if it exceeds the limit
void growh() {
    totalhsize += hpagesize;
    char **newhistory = malloc(sizeof(char*) * totalhsize);
    for(int i = 0; i < historycount; i++) {
        *(newhistory + i) = *(history + i);
    }
    free(history);
    history = newhistory;
    newhistory = NULL;
}
void addh(char* stringhistory) {
    if(historycount == totalhsize) growh();
    char** curhist = history + historycount;
    *curhist = malloc(sizeof(char) * (strlen(stringhistory) + 1));
    strcpy(*curhist, stringhistory);
    historycount++;
}

void run(char *buffer) {
    if(strcmp(buffer, "\n") == 0) return;
    buffer = strtok(buffer, "\n");
    addh(buffer);
    buffer = strtok(buffer, "\t");
    int argcount = 0;
    int bufferlength = strlen(buffer);
    char **commands = malloc(sizeof(char*) * (bufferlength/2 + 2));
    char *token = strtok(buffer, " ");
    if(token == NULL) return;
    while(token != NULL) {
        *(commands + argcount) = malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(*(commands + argcount), token);
        argcount ++;
        token = strtok(NULL, " ");
    }
    *(commands + argcount) = NULL;
    argcount --; //don't count process name (commands[0])
    int mode = 0;
    int pipeindex = -1;
    for(int a = 0; a < argcount + 1; a++) {
        char* token = *(commands + a);
        if(strcmp(token, ">") == 0) {
            if(mode != 0) {
                error(); mode = -1; break;
            }
            mode = 1;
            if(a != argcount - 1) {
                error(); mode = -1; break;
            }
        }
        else if(strcmp(token, "|") == 0) {
            if(mode != 0 || a == 0 || a == argcount + 1) {
                error(); mode = -1; break;
            }
            mode = 2;
            pipeindex = a;
        }
    }
    char **command1st;
    char **command2nd;
    if(mode > -1) {
        //pipe
        if(mode == 2) {
            command1st = malloc(sizeof(char*) * pipeindex + 1);
            command2nd = malloc(sizeof(char*) * (argcount - pipeindex + 1));
            for(int b = 0; b < pipeindex; b++) {
                *(command1st + b) = *(commands + b);
            }
            *(command1st + pipeindex) = NULL;
            for(int c = 0; c < argcount - pipeindex; c++) {
                *(command2nd + c) = *(commands + c + pipeindex + 1);
            }
            *(command2nd + argcount - pipeindex) = NULL;
            free(commands);
            commands = command1st;
            command1st = NULL;
        }
        // run command
        if(strcmp(*commands, "exit") == 0) {
            if(argcount > 0) {
                error(); return;
            }
            for(int i = 0; i < pathcount; i++) {
                free(*(pathspace + i));
            }
            free(pathspace);
            pathspace = NULL;
            for(int j = 0; j < historycount; j++) {
                free(*(history + j));
            }
            free(history);
            history = NULL;
            exit(0);
        }
        else if(strcmp(*commands, "cd") == 0) {
            if(argcount == 0 || argcount > 1) {
                error(); return;
            }
            if(chdir(*(commands + 1)) == -1) {
                error(); return;
            }
        }
        else if(strcmp(*commands, "history") == 0) {
            int size = historycount;
            if(argcount > 1) {
                error(); return;
            }
            if(argcount == 1) {
                char* ssize = *(commands + 1);
                int slength = strlen(ssize);
                double dsize = atof(ssize);
                char* output = malloc(sizeof(char) * (slength + 1));
                snprintf(output, slength + 1, "%f", dsize);
                if(strcmp(ssize, output) != 0) {
                    error(); return;
                }
                size = ceil(dsize);
                free(output);
                output = NULL;
            }
            int start = 0;
            if(size < historycount) start = historycount - size;
            for(int i = start; i < historycount; i++) {
                printf("%s\n", *(history + i));
            }
        }
        else if(strcmp(*commands, "path") == 0) {
            for(int i = 0; i < pathcount; i++) {
                free(*(pathspace + i));
            }
            free(pathspace);
            pathspace = NULL;
            pathspace = malloc(sizeof(char*) * argcount);
            pathcount = argcount;
            for(int i = 0; i < argcount; i++) {
                char** curpath = (pathspace + i);
                char* curarg = *(commands + (i + 1));
                char last = *(curarg + (strlen(curarg) - 1));
                if(last != '/') {
                    strcat(curarg, "/");
                }
                *curpath = malloc(sizeof(char) * (strlen(curarg) + 1));
                strcpy(*curpath, curarg);
            }
        }
        else {
            int pipefd[2];
            pipe(pipefd);
            int children = 0;
            for(int i = 0; i < pathcount; i++) {
                char* path = *(pathspace + i);
                char* filename = *commands;
                char* fullpath = malloc(sizeof(char) * (strlen(path) + strlen(filename) + 1));
                strcpy(fullpath, path);
                strcat(fullpath, filename);
                filename = NULL;
                if(access(fullpath, X_OK) == 0) {
                    int cpid = fork();
                    if(cpid < 0) {
                        error(); return;
                    }
                    else {
                        if(cpid == 0) {
                            int fd = 0;
                            if(mode == 1) {
                                fd = open(*(commands + argcount), O_RDWR|O_CREAT|O_TRUNC, 0600);
                                free(*(commands + argcount));
                                *(commands + argcount) = NULL;
                                free(*(commands + argcount - 1));
                                *(commands + argcount - 1) = NULL;
                                argcount -= 2;
                                if(fd == -1) error();
                                if(dup2(fd, 1) == -1) error();
                            }
                            if(mode == 2) {
                                close(pipefd[0]);
                                dup2(pipefd[1], STDOUT_FILENO);
                                close(pipefd[1]);
                            }
                            if(execv(fullpath, commands) == -1) {
                                error(); exit(1);
                            }
                            if(mode == 1) {
                                close(fd);
                                dup2(2, 1);
                            }
                            free(fullpath);
                            fullpath = NULL;
                            exit(0);
                        }
                        else {
                            if(mode == 2) {
                                int gchildren = 0;
                                for(int e = 0; e < pathcount; e++) {
                                    char* path2 = *(pathspace + e);
                                    char* filename2 = *command2nd;
                                    char* fullpath2 = malloc(sizeof(char) * (strlen(path2) + strlen(filename2) + 1));
                                    strcpy(fullpath2, path2);
                                    strcat(fullpath2, filename2);
                                    filename2 = NULL;
                                    if(access(fullpath2, X_OK) == 0) {
                                        int cpid2 = fork();
                                        if(cpid2 < 0) {
                                            error(); return;
                                        }
                                        else {
                                            if(cpid2 == 0) {
                                                close(pipefd[1]);
                                                dup2(pipefd[0], STDIN_FILENO);
                                                close(pipefd[0]);
                                                if(execv(fullpath2, command2nd) == -1) {
                                                    error(); exit(1);
                                                }
                                                free(fullpath2);
                                                fullpath2 = NULL;
                                                exit(0);
                                            }
                                            else {
                                                close(pipefd[0]);
                                                close(pipefd[1]);
                                                gchildren++;
                                                wait(NULL);
                                            }
                                        }
                                    }
                                }
                                if(!gchildren) {
                                    error(); return;
                                }
                                for(int d = 0; d < argcount - pipeindex; d++) {
                                    free(*(command2nd + d));
                                }
                                free(command2nd);
                                command2nd = NULL;
                            }
                        }
                        close(pipefd[0]);
                        close(pipefd[1]);
                        children++;
                        wait(NULL);
                    }
                }
            }
            if(!children) {
                error(); return;
            }
        }
    }
    if(mode != 2) {
        for(int k = 0; k < argcount + 1; k++) {
            free(*(commands + k));
        }
        free(commands);
        commands = NULL;
    }
}
char* defaultpath = "/bin/";
void init() {
    pathspace = malloc(sizeof(char*));
    *pathspace = malloc(sizeof(char) * (strlen(defaultpath) + 1));
    strcpy(*pathspace, defaultpath);
    pathcount = 1;

    history = malloc(sizeof(char*) * hpagesize);
    historycount = 0;
    totalhsize = hpagesize;
}
int main(int argc, char* argv[]) {
    if(argc > 2) {
        error(); exit(1);
    }
    size_t str_size = 0;
    char* buffer = NULL;
    init();
    if(argc == 1) {
        while(1) {
            printf("wish> ");
            fflush(stdout);
            //read arg
            if(getline(&buffer, &str_size, stdin) == -1) {
                error();
            }
            run(buffer);
        }
    }
    else {
        FILE *fp = fopen(argv[1], "r");
        if(fp == NULL) {
            error(); exit(1);
        }
        while(getline(&buffer, &str_size, fp) != -1) {
            run(buffer);
        }
    }
    exit(0);
}
