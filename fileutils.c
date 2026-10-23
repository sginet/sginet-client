#include "include/cloudAPI.h"
#include "include/fileutils.h"
#include <time.h>

#define SCRSAVE_TMPFILE "/tmp/scrsave.rgb"
#define SCRSAVE_TMPSCRIPT "/tmp/_sginet-screenshot.sh"

static void setFormField(HttpFormField *f, const char *name, const char *value, int isFile) {
    f->name = name;
    f->value = value;
    f->isFile = isFile;
}

int uploadS3(char* fileToUpload, char* contentType, char* s3key, S3Policy* policy) {
    HttpFormField fields[11];
    int status;
    if (debug) fprintf(stderr, "Upload URL: %s\n", policy->url);
    status = 0;

    /* fields from policy, then regular fields (same order as the former curl form) */
    /* our bucket doesn't use ACLs: {"acl", "public-read", 0} */
    /* C89: no non-constant initializers for local arrays */
    setFormField(&fields[0], "success_action_status", policy->actionStatus, 0);
    setFormField(&fields[1], "bucket", policy->bucket, 0);
    setFormField(&fields[2], "X-Amz-Algorithm", policy->algorithm, 0);
    setFormField(&fields[3], "X-Amz-Credential", policy->credential, 0);
    setFormField(&fields[4], "X-Amz-Date", policy->date, 0);
    setFormField(&fields[5], "X-Amz-Security-Token", policy->token, 0);
    setFormField(&fields[6], "Policy", policy->policy, 0);
    setFormField(&fields[7], "X-Amz-Signature", policy->signature, 0);
    setFormField(&fields[8], "key", s3key, 0);
    setFormField(&fields[9], "file", fileToUpload, 1);
    setFormField(&fields[10], "content-type", contentType, 0);

    if (http_post_multipart(policy->url, fields, sizeof(fields) / sizeof(fields[0]), &status) != HTTP_OK) {
        fprintf(stderr, "Upload failed: %s\n", http_last_error());
        return 1;
    }
    if (status < 200 || status > 299) {
        fprintf(stderr, "Upload failed: HTTP status %d\n", status);
        return 1;
    }
    return 0;
}
int saveScreenshotToFile(int screenshotMode, int x1, int x2, int y1, int y2) {
    FILE *fp = NULL;
    char command[999];

    /* we have to create a temporary shell script to set ND_LIBRARYN32_PATH with /usr/lib32, or scrsave will pick the SGUG library and fails */
    if (access( SCRSAVE_TMPSCRIPT, F_OK ) != 0 ) {
        FILE *fp;
        if (debug) fprintf(stderr, "Creating tmp script: %s\n", SCRSAVE_TMPSCRIPT);
        fp = fopen(SCRSAVE_TMPSCRIPT, "w");
        fprintf(fp, "#!/sbin/csh\nsetenv LD_LIBRARYN32_PATH /usr/lib32\n/usr/sbin/scrsave /tmp/scrsave.rgb $1 $2 $3 $4 $5\n");
        fclose(fp);
        chmod(SCRSAVE_TMPSCRIPT, S_IRWXU|S_IRGRP|S_IRWXG|S_IROTH|S_IXOTH);
    }
/* contents of tmpExec:
    #!/sbin/csh
    setenv LD_LIBRARYN32_PATH /usr/lib32
    /usr/sbin/scrsave /tmp/scrsave.rgb $1 $2 $3 $4
*/
    if (screenshotMode == 1) { /* SCREENSHOT_REGION */
        sprintf(command, "/sbin/csh -f %s %i %i %i %i", SCRSAVE_TMPSCRIPT, x1, x2, y1, y2);
    } else { /* SCREENSHOT_FULLSCREEN */
        sprintf(command, "/sbin/csh -f %s", SCRSAVE_TMPSCRIPT);
    }
    if (debug) fprintf(stderr, "Execute: %s\n",command);
    
    if ((fp = popen(command, "r")) != NULL) pclose(fp);

    /* check if output file exists */
    if( access( SCRSAVE_TMPFILE, F_OK ) != 0 ) {
        printf("Screenshot failed. No output file found.\n");
        return 1;
    }
    return 0; /* OK */
}
int takeScreenshotAndUpload(int screenshotMode, int x1, int x2, int y1, int y2) {
    char s3Key[512];
    struct tm tm;
    S3Policy *uploadPolicy;
    time_t t;
    int rc1, rc2, rc3;
    
    /* take screenshot, with delay */
    rc1 = saveScreenshotToFile(screenshotMode, x1, x2, y1, y2);
    if (rc1 != RESPONSE_OK) return rc1;

    /* get signed upload policy */
    rc2 = cloudAPI_getSignedUploadPolicy("scrsave", &uploadPolicy);
    if (rc2 != RESPONSE_OK) return rc2;

    if (debug) fprintf(stderr, "uploadkey: %s\n", uploadPolicy->uploadkey);

    /* generate timestamp for uploaded file */
    t = time(NULL);
    tm = *localtime(&t);

    sprintf(s3Key, "%s%d-%02d-%02d-%02d%02d-%02d.rgb", uploadPolicy->uploadkey, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    if (debug) fprintf(stderr, "s3Key: %s\n", s3Key);
    
    /* upload! */
    rc2 = uploadS3(SCRSAVE_TMPFILE, "image/rgb", s3Key, uploadPolicy);

    /* cleanup */

    return rc2;
}
