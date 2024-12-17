// file utilities.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_COUNT 512
#define LOGFILE "/tmp/olle.txt"

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef SIMULATE
#define SIMULATE 0
#endif

FILE *logfile;


int writelog( const char * message)
{
	if (logfile == NULL)
	{
		printf("Open logfile\n");
		logfile = fopen(LOGFILE, "a");
		if ( logfile == NULL)
		{
			fprintf(stderr, "Unable to open/create logfile\n");
			return(-1);
		}
	}
	fprintf(logfile, "%s", message);
	return(0);
}


int logging(const char* tag, const char* message, int err) {
	time_t now;
	struct tm _calendar_time;
	char _buf[MAX_COUNT];
	char logmessage[1024];

	time(&now);
	localtime_r(&now,&_calendar_time);

	strftime(_buf, MAX_COUNT, "%c", &_calendar_time);
	snprintf(logmessage, sizeof(logmessage),"%s [%s]: %s errno: %s\n", _buf, tag, message, strerror(err));
	printf("Err: %d\n", err);
	return(writelog(logmessage));
	
}


int get_temperature(char *sensor, float *ret_temp)
{
        char buf[100];
        char path[100];
        int fd =-1;
        char *temp;


        if ( SIMULATE == 1 )
	{
		sprintf(path, "simulate/%s", sensor);
	} else {
		sprintf(path, "/sys/bus/w1/devices/%s/w1_slave", sensor);
	}
        // Open the file in the path.
        if((fd = open(path,O_RDONLY)) < 0)
        {
                perror("open ds18b20\n");
                return 1;
        }
        // Read the file
        if(read(fd,buf,sizeof(buf)) < 0)
        {
                perror("read ds18b20\n");
                return 1;
        }
        // Returns the first index of 't'.
        temp = strchr(buf,'t');
        // Read the string following "t=".
        sscanf(temp,"t=%s",temp);
        // atof: changes string to float.
        *ret_temp = atof(temp)/1000;
        if (DEBUG)
                printf(" temp : %3.3f °C\n", *ret_temp);
        close(fd);

        return 0;
}

// Handle signals
void sigint_handler(int signal)
{
        if (signal == SIGINT)
                printf("\nIntercepted SIGINT!\n");
        if (signal == SIGHUP)
                printf("\nIntercepted SIGHUP!\n");
        if (signal == SIGTERM)
                printf("\nIntercepted SIGTERM!\n");
        exit(1);
}

void set_signal_action(void)
{
        // Declare the sigaction structure
        struct sigaction act;

        // Set all of the structure's bits to 0 to avoid errors
        // relating to uninitialized variables...
        bzero(&act, sizeof(act));
        // Set the signal handler as the default action
        act.sa_handler = &sigint_handler;
        // Apply the action in the structure to the
        // SIGINT signal (ctrl-c)
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGHUP, &act, NULL);
        sigaction(SIGTERM, &act, NULL);
}

