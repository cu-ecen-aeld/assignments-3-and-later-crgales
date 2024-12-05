#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    FILE *fp;
    
    if (argc < 3) {
        printf("Usage: %s <FILE> <CONTENTS>", argv[0]);
        exit(-1);
    }

    syslog(LOG_USER, "Writing %s to %s", argv[2], argv[1]);

    fp = fopen(argv[1], "w");

    if (fp == NULL) {
        perror("Error creating file");
        exit(-1);
    }

    fprintf(fp, "%s", argv[2]);

    fclose(fp);
}