#include<stdio.h>
#include<stdlib.h>
#include<string.h>

int main(int argc, char *argv[]) {
    size_t str_size = 0;
    char *buffer = NULL;
    for (int curf = 1; curf < argc; curf ++) {
        FILE *fp = fopen(argv[curf], "r");
        if (fp == NULL) {
            printf("my-cat: cannot open file\n");
            exit(1);
        }
        while (getline(&buffer, &str_size, fp) != -1) {
            printf("%s", buffer);
        }
        if (fclose(fp) != 0) {
            printf("Error: filed to close file %s \n", argv[curf]);
            exit(1);
        }
    }
    free(buffer); buffer = NULL;
    return 0;
}
