/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2025 Christopher Gilliard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#include <alloc.H>
#include <env.H>
#include <misc.H>

#ifndef MEMSAN
#define MEMSAN 0
#endif /* MEMSAN */

extern u8 **environ;

static int env_count(void) {
	int count = 0;
	if (environ) {
		while (environ[count]) count++;
	}
	return count;
}

void init_environ(void) {
	int count = env_count();
	int i;

	/* Allocate new environ array */
	u8 **newenv = alloc(sizeof(u8 *) * (count + 1));
	if (!newenv) return; /* Handle allocation failure gracefully */

	/* Copy each string */
	for (i = 0; i < count; i++) {
		int len = strlen(environ[i]) + 1; /* Include null terminator */
		newenv[i] = alloc(len);
		if (!newenv[i]) {
			/* Cleanup on failure */
			while (i > 0) release(newenv[--i]);
			release(newenv);
			return;
		}
		memcpy(newenv[i], environ[i], len);
	}
	newenv[count] = NULL;

	/* Replace global environ */
	environ = newenv;

#if MEMSAN == 1
	reset_allocated_bytes();
#endif
}

u8 *getenv(const u8 *name) {
	u8 **env;
	if (!name || !environ) return 0;

	for (env = environ; *env; env++) {
		u8 *str = *env;
		int i = 0;
		while (name[i] && str[i] && name[i] == str[i] && str[i] != '=')
			i++;
		if (name[i] == 0 && str[i] == '=') {
			return str + i + 1;
		}
	}
	return 0;
}

int setenv(const u8 *name, const u8 *value, int overwrite) {
	u8 *existing, *new_entry, **new_environ;
	int name_len, value_len, entry_len, i, count;

	if (!name || !*name || strchr(name, '=')) return -1;

	existing = getenv(name);
	if (existing && !overwrite) return 0;

	name_len = strlen(name);
	value_len = strlen(value);
	entry_len = name_len + value_len + 2;
	new_entry = alloc(entry_len);

	if (!new_entry) return -1;

	memcpy(new_entry, name, name_len);
	new_entry[name_len] = '=';
	memcpy(new_entry + name_len + 1, value, value_len);
	new_entry[entry_len - 1] = '\0';

	if (existing) {
		for (i = 0; environ[i]; i++) {
			if (environ[i] == existing - (name_len + 1)) {
				release(environ[i]);
				environ[i] = new_entry;
				return 0;
			}
		}
	} else {
		count = env_count();
		new_environ = alloc((count + 2) * sizeof(u8 *));
		if (!new_environ) {
			release(new_entry);
			return -1;
		}

		if (environ) {
			memcpy(new_environ, environ, count * sizeof(u8 *));
			release(environ);
		}
		new_environ[count] = new_entry;
		new_environ[count + 1] = 0;
		environ = new_environ;
	}
	return 0;
}

int unsetenv(const u8 *name) {
	u8 *existing;
	int count, i;
	if (!name || !*name || strchr(name, '=')) return -1;

	existing = getenv(name);
	if (!existing) return 0;

	count = env_count();
	for (i = 0; environ[i]; i++) {
		if (environ[i] == existing - (strlen(name) + 1)) {
			release(environ[i]);
			memorymove(&environ[i], &environ[i + 1],
				   (count - i) * sizeof(u8 *));
			return 0;
		}
	}
	return 0;
}

