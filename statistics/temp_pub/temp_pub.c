/*
 * Reads up to 10 18ds20 temperature sensors and publishes using mqtt
 */

#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

#define MAXSENSORS 10
#define MAXMAPS 10
#define TOPIC "heater/temperatures"
#define DEBUG 1

char w1_address[20];

struct map
{
	char id[20];
	char name[10];
};

struct map maps[MAXMAPS];

void setupmap() {
	strcpy(maps[0].id, "28-000009ff2514");
	strcpy(maps[0].name, "Retur");
	strcpy(maps[1].id, "28-00000b5450f9");
	strcpy(maps[1].name, "Framl");
	strcpy(maps[2].id, "28-00000b5b88a3");
	strcpy(maps[2].name, "Pump");
	strcpy(maps[3].id, "28-000000384a6d");
	strcpy(maps[3].name, "Vatten");
	strcpy(maps[4].id, "28-00000039122b");
	strcpy(maps[4].name, "Inne");
	strcpy(maps[5].id, "28-00000b5c6f2a");
	strcpy(maps[5].name, "Ute");
	printf("%s\n", maps[4].id);
};

int findname(char *sensor){ 
	int i=0;

	while ( i < MAXMAPS )
	{
		// printf("findname: %s %s\n", sensor, maps[i].id);
		if (strcmp(sensor, maps[i].id) == 0)
		{
			return(i);
		}
		i++;
	}
	return(-1);
}

int scan_for_w1(char sensors[MAXSENSORS][20]) {
	DIR *dirp;
	struct dirent *direntp;
	char *w1_path = "/sys/bus/w1/devices";
	int n, numsensors=0;

	if((dirp = opendir(w1_path)) == NULL) {
		printf("opendir error\n");
		return 0;
	}

	n=0;
	while((direntp = readdir(dirp)) != NULL) {
		if (n < MAXSENSORS)
		{
			if(strstr(direntp->d_name,"28-00000") != NULL) {
				strcpy(w1_address,direntp->d_name);
				strcpy(sensors[numsensors],w1_address);
				if (DEBUG)
					printf("Found w1 device: %s \n",sensors[numsensors]);
				numsensors++;
				n++;
			}
		}
	}
	return numsensors;
}


/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	/* Print out the connection result. mosquitto_connack_string() produces an
	 * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
	 * clients is mosquitto_reason_string().
	 */
	if (DEBUG)
		printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		/* If the connection fails for any reason, we don't want to keep on
		 * retrying in this example, so disconnect. Without this, the client
		 * will attempt to reconnect. */
		mosquitto_disconnect(mosq);
	}

	/* You may wish to set a flag here to indicate to your application that the
	 * client is now connected. */
}


/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
void on_publish(struct mosquitto *mosq, void *obj, int mid)
{
	if (DEBUG)
		printf("Message with mid %d has been published.\n", mid);
}


int get_temperature(char *sensor, float *ret_temp)
{
	char buf[100];
	char path[100];
	int fd =-1;
	char *temp;


	sprintf(path, "/sys/bus/w1/devices/%s/w1_slave", sensor);
	// Open the file in the path.
	if((fd = open(path,O_RDONLY)) < 0)
	{
		printf("open error\n");
		return 1;
	}
	// Read the file
	if(read(fd,buf,sizeof(buf)) < 0)
	{
		printf("read error\n");
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

/* This function gets temp values from all sensors, assembles a JSON and publishes it */
int publish_sensor_data(struct mosquitto *mosq, char sensors[MAXSENSORS][20], int numsensors)
{
	char payload[512], probe[20];
	/* int temp; */
	float temp=0;
	int rc, sensor,i;

	for(sensor=0; sensor < numsensors; sensor++)
	{
		if ( get_temperature(sensors[sensor], &temp) == 1)
		{
			return 1;
		}
		if (DEBUG)
			printf("sensor: %d %3.3f\n", sensor,  temp);

		if ( (i = findname( sensors[sensor])) > -1)
		{
			strcpy(probe, maps[i].name);
		} else {
			fprintf(stderr, "unknown sensor %s\n", sensors[sensor]);
			strcpy(probe, "Okänd");
		}

		if ( sensor == 0 && numsensors > 1)
		{
			// Start, multiple sensors
			snprintf(payload, sizeof(payload), "{ \"%s\": \"%3.3f\" , ", probe, temp);
		}
		else if ( sensor == 0 && numsensors == 1)
		{
			// Start and stop, single sensor
			snprintf(payload, sizeof(payload), "{ \"%s\": \"%3.3f\" }", probe, temp);

		}
		else if (sensor == numsensors-1)
		{
			// The last one of multiple sensors
			snprintf(payload+strlen(payload), sizeof(payload), "\"%s\": \"%3.3f\" }", probe, temp);
		}
		else
		{
			// Middle sensors
			snprintf(payload+strlen(payload), sizeof(payload), "\"%s\": \"%3.3f\" ,", probe, temp);
		}
		
	}

	/* Publish the message
	 * mosq - our client instance
	 * *mid = NULL - we don't want to know what the message id for this message is
	 * topic = "example/temperature" - the topic on which this message will be published
	 * payloadlen = strlen(payload) - the length of our payload in bytes
	 * payload - the actual payload
	 * qos = 2 - publish with QoS 2 for this example
	 * retain = false - do not use the retained message feature for this message
	 */
	rc = mosquitto_publish(mosq, NULL, TOPIC , strlen(payload), payload, 2, false);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct mosquitto *mosq;
	int rc;
	/* temp */
	char sensors[MAXSENSORS][20];
	int numsensors;

	setupmap();


	numsensors=scan_for_w1( sensors);
	if ( numsensors < 1)
	{
		fprintf(stderr,"%s: No sensors found\n", argv[0]);
		exit(-1);
	}
	printf("Numsensors; %d\n", numsensors);

	// findsensor(maps[0].id, sensors, numsensors);

	/* Required before calling other mosquitto functions */
	mosquitto_lib_init();

	/* Create a new client instance.
	 * id = NULL -> ask the broker to generate a client id for us
	 * clean session = true -> the broker should remove old sessions when we connect
	 * obj = NULL -> we aren't passing any of our private data for callbacks
	 */
	mosq = mosquitto_new(NULL, true, NULL);
	if(mosq == NULL){
		fprintf(stderr, "Error: Out of memory.\n");
		exit(-1);
	}

	/* Configure callbacks. This should be done before connecting ideally. */
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_publish_callback_set(mosq, on_publish);

	/* Connect to mosquitto server on port 1883, with a keepalive of 60 seconds.
	 * This call makes the socket connection only, it does not complete the MQTT
	 * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
	 * mosquitto_loop_forever() for processing net traffic. */
	/* rc = mosquitto_connect(mosq, "test.mosquitto.org", 1883, 60); */
	rc = mosquitto_connect(mosq, "192.168.0.18", 8883, 60);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		exit(-1);
	}

	/* Run the network loop in a background thread, this call returns quickly. */
	rc = mosquitto_loop_start(mosq);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		exit(-1);
	}

	/* At this point the client is connected to the network socket, but may not
	 * have completed CONNECT/CONNACK.
	 * It is fairly safe to start queuing messages at this point, but if you
	 * want to be really sure you should wait until after a successful call to
	 * the connect callback.
	 * In this case we know it is 1 second before we start publishing.
	 */

	while(1){
		if (publish_sensor_data(mosq, sensors, numsensors) == 1)
		{
			mosquitto_lib_cleanup();
			exit(-1);
		}
		sleep(300);
	}

	exit(0);
}
