#ifndef HEADER_FILEUTILS
#define HEADER_FILEUTILS

#include <sys/stat.h>   /* for file operations */
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

extern int GUI_MODE; 

int takeScreenshotAndUpload(int screenshotMode, int x1, int x2, int y1, int y2);
int saveScreenshotToFile(int screenshotMode, int x1, int x2, int y1, int y2);
int uploadS3(char* fileToUpload, char* contentType, char* s3key, S3Policy* policy);

#endif /* HEADER_FILEUTILS */
