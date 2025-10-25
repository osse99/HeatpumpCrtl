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
#include <mariadb/mysql.h>

#define TOPIC "heater/history"
#define SUBTOPIC "heater/pong"
#define DEBUG 1


/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
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

        /* Making subscriptions in the on_connect() callback means that if the
         * connection drops and is automatically resumed by the client, then the
         * subscriptions will be recreated when the client reconnects. */
        rc = mosquitto_subscribe(mosq, NULL, SUBTOPIC, 1);
        if(rc != MOSQ_ERR_SUCCESS){
                fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
                /* We might as well disconnect if we were unable to subscribe */
                mosquitto_disconnect(mosq);
        }

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



/* This function gets temp values from all sensors, assembles a JSON and publishes it */
int publish_sensor_data(struct mosquitto *mosq, char *ret_topic )
{

        MYSQL *conn;
        MYSQL_RES *result, *result2;
        MYSQL_ROW row, row2;

	int rc;
        int num_fields, num_rows, num_rows2;
        int flag=0;

        char buf[512]="";
        char querystr[512]="";
        char timestamp[10];

	if (DEBUG)
	{
		printf("ret_topic: %s\n", ret_topic);
	}
        conn = mysql_init(NULL);
        mysql_real_connect(conn, "192.168.x.x", "dbuser", "dpuserpw", "db", 0, NULL, 0);

        if ( mysql_query(conn, "select distinct time1 from (select id,time1 from temp order by id desc limit 90) as sub order by id asc"))
        {
                printf("Error Select %u: %s\n", mysql_errno(conn), mysql_error(conn));
                mysql_close(conn);
                exit(1);
        }

        result = mysql_store_result(conn);

        num_fields = mysql_num_fields(result);
        printf("Num_fields: %d\n",num_fields);
        printf("Number of rows: %d\n", (int )mysql_num_rows(result));

        // Are there any rows selected?
	if ( (num_rows=mysql_num_rows(result)) > 0)
        {
                // Skip field name row
                row = mysql_fetch_row(result);

                // Loop through the results rows
                while ((row = mysql_fetch_row(result)))
                {
			snprintf(querystr, sizeof(querystr), "select * from temp where time1 = '%s'", row[0]); 
			printf("QUerystr %s\n", querystr);
        		if ( mysql_query(conn, querystr))
        		{
                		printf("Error Select %u: %s\n", mysql_errno(conn), mysql_error(conn));
                		mysql_close(conn);
                		exit(1);
        		}
        		result2 = mysql_store_result(conn);
			printf("Query run\n");
			if ( (num_rows2=mysql_num_rows(result2)) > 0)
        		{
				printf("Num rows %d\n", num_rows2);
                		// Skip field name row
                		row2 = mysql_fetch_row(result2);

		                // Loop through the results rows
               			while ((row2 = mysql_fetch_row(result2)))
                		{
                                	if (flag == 0)
					{
printf("Row2[1] %s\n", row2[1]);
						// Get timestamp
                                		if( strlen(row2[1]) == 19)
                               			{
                                        		strcpy(timestamp, row2[1]+11);
                                		}
printf("Timestamp %s\n", timestamp);
                                		// First probe row
                                		snprintf(buf, sizeof(buf), "{ \"timestamp\": \"%s\", \"%s\": \"%s\"  ", timestamp, row2[2], row2[3]);
                                		flag=1;
					} else {
                                		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ", \"%s\": \"%s\" ", row2[2], row2[3]);
					}

				}
                              	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf)," }");
				printf("Buf %s\n", buf);
				rc = mosquitto_publish(mosq, NULL, ret_topic , strlen(buf), buf, 2, false);
				if(rc != MOSQ_ERR_SUCCESS){
					fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
					return 1;
				}
				flag=0;
                        }
                }
		if (DEBUG)
                	printf("\n");
        }
        mysql_free_result(result);
        mysql_free_result(result2);
        mysql_close(conn);

	return 0;
}

/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
        /* This blindly prints the payload, but the payload can be anything so take care. */
        printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
		if (publish_sensor_data(mosq, (char *)msg->payload ) == 1)
		{
			mosquitto_lib_cleanup();
			exit(-1);
		}
}


int main(int argc, char *argv[])
{
	struct mosquitto *mosq;
	int rc;
	/* temp */

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
	mosquitto_message_callback_set(mosq, on_message);

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
//		if (publish_sensor_data(mosq) == 1)
//		{
//			mosquitto_lib_cleanup();
//			exit(-1);
//		}
		sleep(3600);
	}

	exit(0);
}
