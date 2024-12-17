

int logging(const char* tag, const char* message, int err);
int get_temperature(char *sensor, float *ret_temp);
void sigint_handler(int signal);
void set_signal_action(void);
