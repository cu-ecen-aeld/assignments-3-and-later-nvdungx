#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int retcode = 0;
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
    if (argc != 3)
    {
        syslog(LOG_ERR, "Error: need 2 inputs <filename> and <string>");
        printf("Usage: %s <filename> <string>\n", argv[0]);
        printf("    The <string> will be write to <filename>");
        retcode = 1;
    }
    else
    {
        const char *filename = argv[1];
        const char *writestr = argv[2];
        int fd = open(filename, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IROTH);
        if (fd < 0)
        {
            syslog(LOG_ERR, "Error: Could not create file %s", filename);
            retcode = 1;
        }
        if (retcode == 0)
        {
            int len = strlen(writestr);
            int writelen = write(fd, writestr, len);
            if (writelen != len)
            {
                syslog(LOG_ERR, "Error: failed to write to file %s", filename);
                retcode = 1;
            }
            else
            {
                syslog(LOG_DEBUG, "Write string '%s' to file %s", writestr, filename);
            }
        }
        if (retcode == 0)
        {
            if (close(fd) == -1)
            {
                syslog(LOG_ERR, "Error: failed to close file %s", filename);
                retcode = 1;
            }
        }
    }
    closelog();
    return retcode;
}