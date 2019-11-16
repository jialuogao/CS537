#include<stdio.h>
#include<stdlib.h>
#include<string.h>

// replace "find" with "replace" in "line"
void mysed(char *line, char *find, char *replace) {
    char *findp = strstr(line, find);
    if (findp != NULL) {
        char *half = malloc(strlen(findp));
        strcpy(half, findp + strlen(find));
        strcpy(findp, "\0");
        strcat(line, replace);
        strcat(line, half);
        free(half); half = NULL;
    }
}

int main(int argc, char *argv[]) {
    size_t str_size = 0;
    char *buffer = NULL;
    if (argc < 3) {
        printf("my-sed: find_term replace_term [file ...]\n");
        exit(1);
    } else if (argc == 3) {
        while (getline(&buffer, &str_size, stdin) != -1) {
            mysed(buffer, argv[1], argv[2]);
            printf("%s", buffer);
        }
    }
    // read file by file
    for (int curf = 3; curf < argc; curf ++) {
        // open file
        FILE *fp = fopen(argv[curf], "r");
        if (fp == NULL) {
            printf("my-sed: cannot open file\n");
            exit(1);
        }

        // read line by line, find word, replace and print
        while (getline(&buffer, &str_size, fp) != -1) {
            mysed(buffer, argv[1], argv[2]);
            printf("%s", buffer);
        }

        // close file
        if (fclose(fp) != 0) {
            printf("Error: filed to close file %s \n", argv[curf]);
            exit(1);
        }
    }
    free(buffer); buffer = NULL;
    return 0;
}
