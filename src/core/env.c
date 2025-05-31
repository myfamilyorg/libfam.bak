extern char **environ;

const char *getenv(const char *name) {
	if (!name || !environ) return 0;

	for (char **env = environ; *env; env++) {
		const char *str = *env;
		int i = 0;
		while (name[i] && str[i] && name[i] == str[i] && str[i] != '=')
			i++;
		if (name[i] == 0 && str[i] == '=') {
			return str + i + 1;
		}
	}
	return 0;
}

/*
// Static storage for environment pointer
static char **my_environ;

// Constructor to capture environ
__attribute__((constructor))
void init(void) {
    my_environ = environ; // Save environ at load time
}

// Function to find an environment variable
const char *get_env_value(const char *name) {
    if (!name || !my_environ) return 0;

    for (char **env = my_environ; *env; env++) {
	const char *str = *env;
	int i = 0;
	while (name[i] && str[i] && name[i] == str[i] && str[i] != '=')
	    i++;
	if (name[i] == 0 && str[i] == '=') {
	    return str + i + 1;
	}
    }
    return 0;
}
*/
