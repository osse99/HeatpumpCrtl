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

// WiringPi hardware pwm pins
#define PWMPIN1 1
#define PWMPIN2 23
#define PWMOFF 5
#define PWM_RANGE 192
#define PWM_CLOCK 200

// Pump max running time/loops/start delay
int loop_delay=30;
int max_pump_running_time=240;
int next_start_delay=1800;
int startup_loops=3;

// Temperature probe MAC address mappings, stupid but just to have something to start with
char t_outdoor[]="w1_bus_master1/28-00000b5c6f2a";
char t_output_to_floor[]="w1_bus_master1/28-00000b5450f9";
char t_return_from_floor[]="w1_bus_master1/28-000009ff2514";
char t_hotwater[]="w1_bus_master2/28-000000384a6d";
char t_pump_output[]="w1_bus_master1/28-00000b5b88a3";
char t_indoor[]="w1_bus_master2/28-00000039122b";

// WiringPi GPIO pins
int valve_pin=25;
int heatpump_pin=28;
int pwmspeed1=120;
int pwmspeed2=120;

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


// Token in config file, pointer to variable, variable type (string, int, float)
typedef struct {
	const char *key;
	void *target;
	char type; // 's' for string, 'i' for int, 'f' for float
} ConfigMap;

// Configurable variable mappings
ConfigMap mappings[] = {
	{"OUTDOOR", t_outdoor, 's'},
	{"OUTPUT_TO_FLOOR", t_output_to_floor, 's'},
	{"RETURN_FROM_FLOOR", t_return_from_floor, 's'},
	{"HOT_WATER", t_hotwater, 's'},
	{"PUMP_OUTPUT", t_pump_output, 's'},
	{"INDOOR", t_indoor, 's'},
	{"VALVE_PIN", &valve_pin, 'i'},
	{"HEATPUMP_PIN", &heatpump_pin, 'i'},
	{"MAX_FLOOR_OUTPUT", &max_floor_output, 'i'},
	{"PUMP_MAX_OUTPUT", &pump_max_output, 'i'},
	{"MIN_TEMP_HOTWATER", &min_t_hotwater, 'i'},
	{"MIN_OUTDOOR", &min_t_outdoor, 'i'},
	{"M_VALUE", &m_value, 'i'},
	{"K_VALUE", &k_value, 'f'},
	{"LOGFILE", logfile_path, 's'},
	{"LOOP_DELAY", &loop_delay, 'i'},
	{"MAX_PUMP_RUNNING_TIME", &max_pump_running_time, 'i'},
	{"NEXT_START_DELAY", &next_start_delay, 'i'},
	{"PWM_SPEED_1", &pwmspeed1, 'i'},
	{"PWM_SPEED_2", &pwmspeed2, 'i'},
	{"STARTUP_LOOPS", &startup_loops, 'i'},
	{NULL, NULL, 0} // End marker
};


int verbose=0;
int verbose_temp=0;
int simulate=0;

// Read the configuration file and override compile time defaults
//

void read_config(char *conffile)
{
	config_t cfg;
	const char *str;
	int result;

	/* Read the config file if it exists */
	/* Override defaults (compiled) */
	result=access(conffile, R_OK);
	if (result == 0)
	{
		config_init(&cfg);
		if(! config_read_file(&cfg, conffile))
		{
			fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
					config_error_line(&cfg), config_error_text(&cfg));
			config_destroy(&cfg);
		} else {
			// Temp sensor MAC adresses
			for (ConfigMap *map = mappings; map->key != NULL; ++map)
			{
				switch (map->type)
				{
					case 's':
						if (config_lookup_string(&cfg, map->key, &str))
						{
							printf("%s: %s\n", map->key, str);
							strncpy((char *)map->target, str, strlen((char *)map->target));
							if (strlen(str) > strlen(map->target))
							{
								fprintf(stderr, "Warning: %s is to long and will be truncated. str: %s truncated: %s\n", map->key, str, (char *)map->target);
							}
						}
						break;
					case 'f':
						if (  config_lookup_float(&cfg, map->key, (double *)map->target) )
						{
							printf("%s: %3.3f\n", map->key, *(double *)map->target);
						}
						break;
					case 'i':
						if (  config_lookup_int(&cfg, map->key, (int *)map->target) )
						{
							printf("%s: %d\n", map->key, *(int *)map->target);
						}
						break;

				}
			}
		}

	} else {
		fprintf(stderr, "No config file exist, skipping and using defaults and args only");
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
	pinMode(PWMPIN1,PWM_OUTPUT);
	pinMode(PWMPIN2,PWM_OUTPUT);
	pwmSetMode(PWM_MODE_MS);
	pwmSetClock(PWM_CLOCK);
	pwmSetRange (PWM_RANGE) ;
	// Stop both pumps
	pwmWrite(PWMPIN1, PWMOFF);
	pwmWrite(PWMPIN2, PWMOFF);

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
	pwmWrite(PWMPIN1,pwmspeed1);
	pwmWrite(PWMPIN2,pwmspeed2);
	sleep(5);
	// Start heatpump
	digitalWrite(heatpump_pin,1);
	// Run until max floor temp
	pump_working=0;
	do
	{
		sleep(loop_delay);
		print_verbose("Start heat run");
		if ( get_temperature(t_output_to_floor, &temp) != 0)
		{	
			log_quit("Get_temperature failed:", t_output_to_floor, 0);
		}
		if ( pump_working == startup_loops)
		{
			floor_temp=temp;
			if (get_temperature(t_pump_output, &temp) != 0)
			{	
				log_quit("Get_temperature failed:", t_pump_output, 0);
			}
			print_verbose("Heat run, checking that pump has started to produce heat");
			if ( floor_temp > temp)
			{
				log_quit("Heat run", "No heat from pump after 3 cycles", 0);
			}
			temp=floor_temp;
		}
		if ( pump_working > max_pump_running_time )
		{
			log_quit("Heat run", "Pump running longer than MAX_PUMP_RUNNNING_TIME, aborting", 0);
		}
		pump_working++;
		print_verbose("Heat run loop");
	}
	while ( temp < max_floor_output );
	logging("Heat run", "Floor is hot, switching to hotwater", 0);
	// Start hot water production
	digitalWrite(valve_pin, 1);	
	do
	{
		sleep(loop_delay);
		print_verbose("Heat run, start hotwater");
		if (get_temperature(t_pump_output, &temp) != 0)
		{	
			log_quit("Get_temperature failed:", t_pump_output, 0);
		}
		if ( pump_working > max_pump_running_time )
		{
			log_quit("Heat run", "Pump running longer than MAX_PUMP_RUNNING_TIME, aborting", 0);
		}
		print_verbose("Heat run, hotwater loop");
		pump_working++;
	}
	while ( temp < pump_max_output );
	// Stop heatpump, reset valve
	digitalWrite(heatpump_pin,0);
	digitalWrite(valve_pin, 0);
	sleep(5);
	// Done shut down
	pwmWrite(PWMPIN1,PWMOFF);
	pwmWrite(PWMPIN2,PWMOFF);
	logging("Heat run", "Hotwater is at pump max temp, done", 0);
	// Delay until next acceptable pump start
	sleep(next_start_delay);
	return(0);
}

// Only heat the hotwater
int start_hotwater_run()
{
	float temp, pump_temp;
	int pump_working;

	logging("Hotwater run", "Starting to heat water", 0);
	// Start circulation
	pwmWrite(PWMPIN1,pwmspeed1);
	pwmWrite(PWMPIN2,pwmspeed2);
	digitalWrite(valve_pin, 1);	
	sleep(5);
	digitalWrite(heatpump_pin,1);
	pump_working=0;
	do
	{
		sleep(loop_delay);
		print_verbose("Hotwater run start");
		if ( get_temperature(t_pump_output, &temp) != 0)
		{	
			log_quit("Get_temperature failed:", t_pump_output, 0);
		}
		if ( pump_working == startup_loops)
		{
			pump_temp=temp;
			if (get_temperature(t_hotwater, &temp) != 0)
			{	
				log_quit("Get_temperature failed:", t_hotwater, 0);
			}
			print_verbose("Hotwater, verifying that pump is producing heat");
			if ( pump_temp < temp)
			{
				log_quit("Heat run", "No heat from pump after 3 cycles", 0);
			}
			temp=pump_temp;
		}
		if ( pump_working > max_pump_running_time )
		{
			log_quit("Hotwater run", "Pump running longer than MAX_PUMP_RUNNING_TIME, aborting", 0);
		}
		pump_working++;
		print_verbose("Hotwater loop");
	}
	while ( temp < pump_max_output );
	// Stop heatpump, reset valve
	digitalWrite(heatpump_pin,0);
	digitalWrite(valve_pin, 0);
	sleep(5);
	// Done shut down
	pwmWrite(PWMPIN1,PWMOFF);
	pwmWrite(PWMPIN2,PWMOFF);
	logging("Hot water run", "Hotwater is at pump max temp setting, done", 0);
	// Delay until next acceptable pump start
	sleep(next_start_delay);
	return(0);
}

// Simple kx+m line, to be enhanced
int calculate_target_temp(float *outdoor_temp, float *ret_temp)
{

	// Y=KX+M inverted
	*ret_temp=m_value - k_value * *outdoor_temp;
	return(0);	
}

// Heat pump should not be operated below -10 Celcius (Nibe F2026)
int to_cold()
{
	float temp;

	if (get_temperature(t_outdoor, &temp))
	{	
		log_quit("Get_temperature failed:", t_outdoor, 0);
	}
	
	print_verbose("in To cold");
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

	if (get_temperature(t_hotwater, &temp))
	{	
		log_quit("Get_temperature failed:", t_hotwater, 0);
	}
	print_verbose("in Hot water");
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

	if (get_temperature(t_return_from_floor, &current_temp))
	{	
		log_quit("Get_temperature failed:", t_return_from_floor, 0);
	}
	if (get_temperature(t_outdoor, &outdoor_temp))
	{	
		log_quit("Get_temperature failed:", t_outdoor, 0);
	}
	print_verbose("in heat, temperatures read");
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
			printf("in heat: needed\n");
		}
		return(1);
	}
	if ( verbose )
	{
		printf("in heat: not needed\n");
	}
	return(0);
}

int main(int argc, char *argv[] )
{
	int daemon=0;
	int opt;
	char *conffile = "pump_controller.cfg";
	char *logfile;

	while ((opt = getopt(argc, argv, "df:l:stvh")) != -1) {
		switch(opt) {
			case 'd' : daemon = 1; break;
			case 'f' : conffile = optarg ; break;
			case 'l' : logfile = optarg ; break;
			case 's' : simulate = 1; break;
			case 'v' : verbose = 1; break;
			case 't' : verbose_temp = 1; break;
			case 'h' : printf("Usage: %s\n%s\n%s\n%s\n%s\n%s\n",\
						   "-d daemonize", 
						   "-f configfile",
						   "-l logfile",
						   "-v verbose",
						   "-t verbose with temp sensor values",
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
	read_config(conffile);

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
		print_verbose("in main loop");
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
		sleep(loop_delay);
	}
}
