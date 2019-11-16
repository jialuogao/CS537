#include<stdio.h>
#include<stdlib.h>
#include<string.h>

int main(int argc, char *argv[]) {
    size_t str_size = 0;
    char *buffernew = NULL;
    char *buffer = malloc(str_size);
    // no file
    if (argc < 2) {
        while (getline(&buffernew, &str_size, stdin) != -1) {
            if (strcmp(buffer, buffernew) != 0) {
                printf("%s", buffernew);
            }
            buffer = realloc(buffer, strlen(buffernew));
            strcpy(buffer, buffernew);
        }
    }
    // read file by file
    for (int curf = 1; curf < argc; curf ++) {
        // open file
        FILE *fp = fopen(argv[curf], "r");
        if (fp == NULL) {
            printf("my-uniq: cannot open file\n");
            exit(1);
        }
        // clear buffer
        strcpy(buffer, "\0");
        // read line by line, find word, replace and print
        while (getline(&buffernew, &str_size, fp) != -1) {
            if (strcmp(buffer, buffernew) != 0) {
                printf("%s", buffernew);
            }
            buffer = realloc(buffer, strlen(buffernew));
            strcpy(buffer, buffernew);
        }
        // close file
        if (fclose(fp) != 0) {
            printf("Error: filed to close file %s \n", argv[curf]);
            exit(1);
        }
    }
    free(buffernew); buffernew = NULL;
    free(buffer); buffer = NULL;
    return 0;
}
