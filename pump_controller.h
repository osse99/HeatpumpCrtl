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
char logfile_path[]="/tmp/olle.log";

