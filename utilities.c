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

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef SIMULATE
#define SIMULATE 0
#endif

FILE *logfile;
extern char logfile_path[];
extern int verbose;
extern int verbose_temp;
extern int simulate;
extern char t_outdoor[], t_output_to_floor[], t_return_from_floor[], t_hotwater[], t_pump_output[], t_indoor[];

int gpio_setup();

int writelog( const char * message)
{
	if (logfile == NULL)
	{
		if (verbose)
			printf("Open logfile\n");
		logfile = fopen(logfile_path, "a");
		if ( logfile == NULL)
		{
			fprintf(stderr, "Unable to open/create logfile\n");
			return(-1);
		}
	}
	fprintf(logfile, "%s", message);
	fflush(logfile);
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
	if ( err != 0 )
	{
		snprintf(logmessage, sizeof(logmessage),"%s\t[%s]: %s\terrno: %s\n", _buf, tag, message, strerror(err));
	} else {
		snprintf(logmessage, sizeof(logmessage),"%s\t[%s]: %s\t-\n", _buf, tag, message);
	}
	return(writelog(logmessage));

}



int get_temperature(char *sensor, float *ret_temp)
{
	char buf[100];
	char path[100];
	int fd =-1;
	char *temp;
	char *endptr;
	float reading;


	if ( simulate )
	{
		sprintf(path, "/tmp/simulate/%s", sensor);
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
	// strof: changes string to float.
	reading = strtof(temp, &endptr);
	if ( *endptr != '\0' )
	{
		perror("can't convert temp string to float");
		return(1);
	}
	*ret_temp = reading/1000;
	// if (verbose)
	//        printf(" temp : %3.3f °C\n", *ret_temp);
	close(fd);

	return 0;
}

void debug_temperature()
{
	float temp;

	get_temperature(t_outdoor, &temp);
	printf("t_outdoor\t\t%s\t%3.3f\n", t_outdoor, temp);

	get_temperature(t_output_to_floor, &temp);
	printf("t_output_to_floor\t%s\t%3.3f\n", t_output_to_floor, temp);

	get_temperature(t_return_from_floor, &temp);
	printf("t_return_from_floor\t%s\t%3.3f\n", t_return_from_floor, temp);

	get_temperature(t_hotwater, &temp);
	printf("t_hotwater\t\t%s\t%3.3f\n", t_hotwater, temp);

	get_temperature(t_outdoor, &temp);
	printf("t_outdoor\t\t%s\t%3.3f\n", t_outdoor, temp);

	get_temperature(t_pump_output, &temp);
	printf("t_pump_output\t\t%s\t%3.3f\n", t_pump_output, temp);

	get_temperature(t_indoor, &temp);
	printf("t_indoor\t\t%s\t%3.3f\n", t_indoor, temp);
}

int log_quit(const char* tag, const char* message, int err)
{
        logging( tag, message, err);
        gpio_setup();
        exit(-1);
}

void print_verbose(char *msg)
{
        if (verbose)
        {
                printf("%s\n", msg);
        }
        if (verbose_temp)
        {
                printf("%s\n", msg);
                debug_temperature();
        }
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
	logging("Terminated", "Process terminated by signal", 0);
	if ( logfile != NULL )
		fclose(logfile);
	// Shut down pumps and valve
	gpio_setup();
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

