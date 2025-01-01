/*  Pump controller 	*/
/* MikroDu 20241206 	*/
/* AO 			*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <wiringPi.h>
#include <stdint.h>
#include <libconfig.h>
#include <sys/stat.h>
#include "utilities.h"

#ifndef DEBUG
#define DEBUG 0
#endif

// Pump max running time/loops/start delay
#define LOOP_DELAY 30
#define MAX_PUMP_RUNNING 240
#define NEXT_START_DELAY 1800
#define STARTUP_LOOPS 3

// Temperature probe MAC address mappings
char t_outdoor[]="w1_bus_master1/28-00000b5c6f2a";
char t_output_to_floor[]="w1_bus_master1/28-00000b5450f9";
char t_return_from_floor[]="w1_bus_master1/28-000009ff2514";
char t_hotwater[]="w1_bus_master2/28-000000384a6d";
char t_pump_output[]="w1_bus_master1/28-00000b5b88a3";
char t_indoor[]="w1_bus_master2/28-00000039122b";

// WiringPi GPIO pins
int valve_pin=25;
int heatpump_pin=28;

// Temperature settings
int max_floor_output=29;
int pump_max_output=50;
int min_t_hotwater=39;
int min_t_outdoor=-10;

// Target temp calculation
int m_value=22;
double k_value=0.2;

// Other
char logfile_path[256];

int verbose=0;
int simulate=0;

// Read the configuration file and override compile time defaults
//
void read_config(char *konffile)
{
	config_t cfg;
	const char *str;
	int result;

	/* Read the config file if it exists */
	/* Override defaults (compiled) */
	result=access(konffile, F_OK);
	if (result == 0)
	{
		config_init(&cfg);
		if(! config_read_file(&cfg, konffile))
		{
			fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
					config_error_line(&cfg), config_error_text(&cfg));
			config_destroy(&cfg);
		} else {
			// Temp sensor MAC adresses
			if ( config_lookup_string(&cfg, "OUTDOOR", &str) ) {
				printf("OUTDOOR: %s\n", str);
				strcpy(t_outdoor, str);
			}
			if ( config_lookup_string(&cfg, "OUTPUT_TO_FLOOR", &str) ) {
				printf("OUT_TO_FLOOR: %s\n", str);
				strcpy(t_output_to_floor, str);
			}
			if ( config_lookup_string(&cfg, "RETURN_FROM_FLOOR", &str) ) {
				printf("RETURN_FROM_FLOOR: %s\n", str);
				strcpy(t_return_from_floor, str);
			}
			if ( config_lookup_string(&cfg, "HOT_WATER", &str) ) {
				printf("HOT_WATER: %s\n", str);
				strcpy(t_hotwater, str);
			}
			if ( config_lookup_string(&cfg, "PUMP_OUTPUT", &str) ) {
				printf("PUMP_OUTPUT: %s\n", str);
				strcpy(t_pump_output, str);
			}
			if ( config_lookup_string(&cfg, "INDOOR", &str) ) {
				printf("INDOOR: %s\n", str);
				strcpy(t_indoor, str);
			}

			// GPIO stuff
			if (  config_lookup_int(&cfg, "VALVEPIN", &valve_pin) ) {
				printf("VALVEPIN: %d\n", valve_pin);
			}
			if (  config_lookup_int(&cfg, "HEATPUMP_PIN", &heatpump_pin) ) {
				printf("HEATPUMP_PIN: %d\n", heatpump_pin);
			}

			// Temperatures
			if (  config_lookup_int(&cfg, "MAX_FLOOR_OUTPUT", &max_floor_output) ) {
				printf("MAX_FLOOR_OUTPUT: %d\n", max_floor_output);
			}
			if (  config_lookup_int(&cfg, "PUMP_MAX_OUTPUT", &pump_max_output) ) {
				printf("PUMP_MAX_OUTPUT: %d\n", pump_max_output);
			}
			if (  config_lookup_int(&cfg, "MIN_TEMP_HOTWATER", &min_t_hotwater) ) {
				printf("MIN_TEMP_HOTWATER: %d\n", min_t_hotwater);
			}
			if (  config_lookup_int(&cfg, "MIN_OUTDOOR", &min_t_outdoor) ) {
				printf("MIN_OUTDOOR: %d\n", min_t_outdoor);
			}

			// Traget temperature calculation
			if (  config_lookup_int(&cfg, "M_VALUE", &m_value) ) {
				printf("M_VALUE: %d\n", m_value);
			}
			if (  config_lookup_float(&cfg, "K_VALUE", &k_value) ) {
				printf("K_VALUE: %3.3f\n", k_value);
			}
			// Other
			if ( config_lookup_string(&cfg, "LOGFILE", &str) ) {
				printf("LOGFILE: %s\n", str);
				strcpy(logfile_path, str);
			}


		}
	} else {
		logging("Readconfig", "No config file exist, skipping and using defaults and args only", 0);
	}
}	

// Set up RaspberryPi ports WiringPi
// Can also be called at program end to shut down pumps and valves
int gpio_setup()
{
	if (wiringPiSetup() == -1)
	{
		logging("WiringPi", "Can't initialize GPIO", 0);
		return(-1) ;
	}

	// Tank circulation pump on PWM 1
	// Heat pump circulation pump on PWM 23
	// PWM 0 and 1 (pin 1, 23) 500Hz range 1-200
	pinMode(1,PWM_OUTPUT);
	pinMode(23,PWM_OUTPUT);
	pwmSetMode(PWM_MODE_MS);
	pwmSetClock(192);
	pwmSetRange (200) ;
	// Stop both pumps
	pwmWrite(1, 5);
	pwmWrite(23, 5);

	// Switch valve relay
	// between heat production (off) and hotwater (on)
	pinMode(valve_pin, OUTPUT);
	digitalWrite(valve_pin, 0);	
	// Heat pump relay
	// simple on/off
	pinMode(heatpump_pin, OUTPUT);
	digitalWrite(heatpump_pin, 0);	
	return(0);
}

// Run to max floor temp and max hotwater temp
int start_heat_run()
{
	float temp, floor_temp;
	int pump_working;

	logging("Heat run", "Starting to heat floor", 0);
	// Start circulation
	pwmWrite(1,120);
	pwmWrite(23,120);
	sleep(5);
	// Start heatpump
	digitalWrite(heatpump_pin,1);
	// Run until max floor temp
	pump_working=0;
	do
	{
		sleep(LOOP_DELAY);
		if (verbose)
		{
			debug_temperature();
		}
		if ( get_temperature(t_output_to_floor, &temp) != 0)
		{	
			logging("Get_temperature failed:", t_output_to_floor, 0);
			gpio_setup();
			exit(-1);
		}
		if ( pump_working == STARTUP_LOOPS)
		{
			floor_temp=temp;
			if (get_temperature(t_pump_output, &temp) != 0)
			{	
				logging("Get_temperature failed:", t_pump_output, 0);
				gpio_setup();
				exit(-1);
			}
			if ( verbose )
			{
				printf("Heat run, check that pump is working temp: %3.3f floor_temp: %3.3f\n", temp, floor_temp);
			}
			if ( floor_temp > temp)
			{
				logging("Heat run", "No heat from pump after 3 cycles", 0);
				gpio_setup();
				exit(-1);
			}
			temp=floor_temp;
		}
		if ( pump_working > MAX_PUMP_RUNNING )
		{
			logging("Heat run", "Pump running longer than MAX_PUMP_RUN, aborting", 0);
			gpio_setup();
			exit(-1);
		}
		pump_working++;
		if ( verbose )
		{
			printf("Heat run, temp: %3.3f\n", temp);
		}
	}
	while ( temp < max_floor_output );
	logging("Heat run", "Floor is hot, switching to hotwater", 0);
	// Start hot water production
	digitalWrite(valve_pin, 1);	
	do
	{
		sleep(LOOP_DELAY);
		if (verbose)
		{
			debug_temperature();
		}
		if (get_temperature(t_pump_output, &temp) != 0)
		{	
			logging("Get_temperature failed:", t_pump_output, 0);
			gpio_setup();
			exit(-1);
		}
		if ( pump_working > MAX_PUMP_RUNNING )
		{
			logging("Heat run", "Pump running longer than MAX_PUMP_RUN, aborting", 0);
			gpio_setup();
			exit(-1);
		}
		if ( verbose )
		{
			printf("Heat run, hotwater, temp: %3.3f\n", temp);
		}
		pump_working++;
	}
	while ( temp < pump_max_output );
	// Stop heatpump, reset valve
	digitalWrite(heatpump_pin,0);
	digitalWrite(valve_pin, 0);
	sleep(5);
	// Done shut down
	pwmWrite(1,5);
	pwmWrite(23,5);
	logging("Heat run", "Hotwater is at pump max temp, done", 0);
	// Delay until next acceptable pump start
	sleep(NEXT_START_DELAY);
	return(0);
}

// Only heat the hotwater
int start_hotwater_run()
{
	float temp, pump_temp;
	int pump_working;

	logging("Hotwater run", "Starting to heat water", 0);
	// Start circulation
	pwmWrite(1,120);
	pwmWrite(23,120);
	digitalWrite(valve_pin, 1);	
	sleep(5);
	digitalWrite(heatpump_pin,1);
	pump_working=0;
	do
	{
		sleep(LOOP_DELAY);
		if (verbose)
		{
			debug_temperature();
		}
		if ( get_temperature(t_pump_output, &temp) != 0)
		{	
			logging("Get_temperature failed:", t_pump_output, 0);
			gpio_setup();
			exit(-1);
		}
		if ( pump_working == STARTUP_LOOPS)
		{
			pump_temp=temp;
			if (get_temperature(t_hotwater, &temp) != 0)
			{	
				logging("Get_temperature failed:", t_hotwater, 0);
				gpio_setup();
				exit(-1);
			}
			if ( verbose )
			{
				printf("Hotwater, verify that pump is working temp: %3.3f hotwater_temp: %3.3f\n", pump_temp, temp);
			}
			if ( pump_temp < temp)
			{
				logging("Heat run", "No heat from pump after 3 cycles", 0);
				gpio_setup();
				exit(-1);
			}
			temp=pump_temp;
		}
		if ( pump_working > MAX_PUMP_RUNNING )
		{
			logging("Hotwater run", "Pump running longer than MAX_PUMP_RUN, aborting", 0);
			gpio_setup();
			exit(-1);
		}
		pump_working++;
		if ( verbose )
		{
			printf("Hotwater run, temp: %3.3f\n", temp);
		}
	}
	while ( temp < pump_max_output );
	// Stop heatpump, reset valve
	digitalWrite(heatpump_pin,0);
	digitalWrite(valve_pin, 0);
	sleep(5);
	// Done shut down
	pwmWrite(1,5);
	pwmWrite(23,5);
	logging("Hot water run", "Hotwater is at pump max temp, done", 0);
	// Delay until next acceptable pump start
	sleep(NEXT_START_DELAY);
	return(0);
}

// Simple kx+m line, to be enhanced
int calculate_target_temp(float *outdoor_temp, float *ret_temp)
{

	// *outdoor_temp=-10;
	// Y=KX+M inverted
	*ret_temp=m_value - k_value * *outdoor_temp;
	return(0);	
}

// Heat pump should not be operated below -10 Celcius (Nibe F2026)
int to_cold()
{
	float temp;

	get_temperature(t_outdoor, &temp);
	if ( verbose )
	{
		printf("in To cold, temp: %3.3f\n", temp);
	}
	if ( temp < min_t_outdoor )
	{	
		logging("To_cold", "Outdoor temperature to low", 0);
		return(1);
	} else {
		return(0);
	}
}

// Hot water needed?
int hot_water()
{
	float temp;

	get_temperature(t_hotwater, &temp);
	if ( verbose )
	{
		printf("Hot water, temp: %3.3f\n", temp);
	}
	if ( temp <= min_t_hotwater )
	{
		return(1);
	}
	logging("Hot_water", "Hot water temp to low", 0);
	return(0);
}

// Heat needed?
int heat()
{
	float current_temp, outdoor_temp, target_temp;

	get_temperature(t_return_from_floor, &current_temp);
	if ( verbose )
	{
		printf("Floor water, temp: %3.3f\n", current_temp);
	}
	get_temperature(t_outdoor, &outdoor_temp);
	if ( verbose )
	{
		printf("Outdoor, temp: %3.3f\n", outdoor_temp);
	}
	calculate_target_temp(&outdoor_temp, &target_temp);
	if ( verbose )
	{
		printf("Target temp: %3.3f\n", target_temp);
	}
	if ( current_temp < target_temp )
	{
		logging("Heat", "Heat needed", 0);
		if ( verbose )
		{
			printf("Heat needed\n");
		}
		return(1);
	}
	if ( verbose )
	{
		printf("No heat needed\n");
	}
	return(0);
}

int main(int argc, char *argv[] )
{
	int daemon=0;
	int opt;
	char *konffile = "pump_controller.cfg";
	char *logfile;

	while ((opt = getopt(argc, argv, "df:l:svh")) != -1) {
		switch(opt) {
			case 'd' : daemon = 1; break;
			case 'f' : konffile = optarg ; break;
			case 'l' : logfile = optarg ; break;
			case 's' : simulate = 1; break;
			case 'v' : verbose = 1; break;
			case 'h' : printf("Usage: %s\n%s\n%s\n%s\n%s\n",\
						   "-d daemonize", 
						   "-f konfigfile",
						   "-l logfile",
						   "-v verbose",
						   "-h help");
				   exit(0);
			default:   fprintf(stderr, "%s: Unknown aargument, -h for help\n", argv[0]); exit(-1);
		}
	}
	// The combination of -d and -v is invalid
	if ( daemon && verbose )
	{
		fprintf(stderr, "%s: Verbose mode does not work when running as a daemon\n", argv[0]);
		exit(-1);
	}

	// No logfile argument so set a default
	if ( strlen(logfile) == 0)
	{
		snprintf(logfile_path, sizeof(logfile_path), "/tmp/%s.log", argv[0]);
	}
	else
	{
		strncpy(logfile_path, logfile, sizeof(logfile_path));
	}
	if ( simulate )
	{
		fprintf(stderr, "%s: Simulation mode selected, reading temperatures from /tmp/simulate directory\n", argv[0]);
	}

	/* Read the config file if it exists */
	/* Override defaults from compile time */
	read_config(konffile);

	// Catch signals
	// Stop pumps and reset switch valve
	set_signal_action();

	// Should we be daemon
	if ( daemon == 1) {
		pid_t pid, sid;
		pid = fork();
		if (pid < 0) { exit(EXIT_FAILURE); }
		if (pid > 0) { exit(EXIT_SUCCESS); }

		umask(0);
		if (setsid ()  == -1) { exit(EXIT_FAILURE); }
		if (chdir("/") < 0) { exit(EXIT_FAILURE); }

		int tmp = open( "/dev/null", O_RDWR);
		dup2(tmp,0);
		dup2(tmp,1);
		dup2(tmp,2);
		close(tmp);
	}

	if (gpio_setup() != 0)
	{
		exit( -1);
	}

	while (1)
	{
		sleep(1);
		if (verbose)
		{
			debug_temperature();
		}
		if ( to_cold() == 1)
		{
			continue;
		}
		if ( heat() == 1)
		{
			start_heat_run();
			continue;
		}
		if ( hot_water() == 1)
		{
			start_hotwater_run();
			continue;
		}
		printf("Not cold and no hot water\n");
		sleep(LOOP_DELAY);
	}
}
