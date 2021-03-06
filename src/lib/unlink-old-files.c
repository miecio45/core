/* Copyright (c) 2002-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "ioloop.h"
#include "str.h"
#include "unlink-old-files.h"

#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>

static int
unlink_old_files_real(const char *dir, const char *prefix, time_t min_time)
{
	DIR *dirp;
	struct dirent *d;
	struct stat st;
	string_t *path;
	size_t prefix_len, dir_len;

	dirp = opendir(dir);
	if (dirp == NULL) {
		if (errno != ENOENT)
			i_error("opendir(%s) failed: %m", dir);
		return -1;
	}

	/* update atime immediately, so if this scanning is done based on
	   atime it won't be done by multiple processes if the scan is slow */
	if (utime(dir, NULL) < 0 && errno != ENOENT)
		i_error("utime(%s) failed: %m", dir);

	path = t_str_new(256);
	str_printfa(path, "%s/", dir);
	dir_len = str_len(path);

	prefix_len = strlen(prefix);
	while ((d = readdir(dirp)) != NULL) {
		if (d->d_name[0] == '.' &&
		    (d->d_name[1] == '\0' ||
		     (d->d_name[1] == '.' && d->d_name[2] == '\0'))) {
			/* skip . and .. */
			continue;
		}
		if (strncmp(d->d_name, prefix, prefix_len) != 0)
			continue;

		str_truncate(path, dir_len);
		str_append(path, d->d_name);
		if (stat(str_c(path), &st) < 0) {
			if (errno != ENOENT)
				i_error("stat(%s) failed: %m", str_c(path));
		} else if (!S_ISDIR(st.st_mode) && st.st_ctime < min_time) {
			i_unlink_if_exists(str_c(path));
		}
	}

	if (closedir(dirp) < 0)
		i_error("closedir(%s) failed: %m", dir);
	return 0;
}

int unlink_old_files(const char *dir, const char *prefix, time_t min_time)
{
	int ret;

	T_BEGIN {
		ret = unlink_old_files_real(dir, prefix, min_time);
	} T_END;
	return ret;
}
