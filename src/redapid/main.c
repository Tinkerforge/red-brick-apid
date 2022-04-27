/*
 * redapid
 * Copyright (C) 2014-2016, 2019 Matthias Bolte <matthias@tinkerforge.com>
 *
 * main.c: RED Brick API Daemon starting point
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>

#include <daemonlib/config.h>
#include <daemonlib/daemon.h>
#include <daemonlib/event.h>
#include <daemonlib/file.h>
#include <daemonlib/log.h>
#include <daemonlib/pid_file.h>
#include <daemonlib/signal.h>
#include <daemonlib/utils.h>

#include "api.h"
#include "cron.h"
#include "inventory.h"
#include "network.h"
#include "process_monitor.h"
#include "version.h"

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

static char _config_filename[1024] = SYSCONFDIR"/redapid.conf";
static char _pid_filename[1024] = LOCALSTATEDIR"/run/redapid.pid";
static char _brickd_socket_filename[1024] = LOCALSTATEDIR"/run/redapid-brickd.socket";
static char _cron_socket_filename[1024] = LOCALSTATEDIR"/run/redapid-cron.socket";
static char _log_filename[1024] = LOCALSTATEDIR"/log/redapid.log";
static File _log_file;
static char _image_version[128] = "<unknown>";
bool _x11_enabled = false;

static void read_image_version(void) {
	FILE *fp;
	int length;

	fp = fopen("/etc/tf_image_version", "rb");

	if (fp == NULL) {
		return;
	}

	length = robust_fread(fp, _image_version, sizeof(_image_version) - 1);

	fclose(fp);

	if (length < 0) {
		string_copy(_image_version, sizeof(_image_version), "<unknown>", -1);

		return;
	}

	_image_version[length] = '\0';

	while (length > 0 && strspn(&_image_version[length - 1], " \f\n\r\t\v") > 0) {
		--length;
	}

	_image_version[length] = '\0';

	if (length == 0) {
		string_copy(_image_version, sizeof(_image_version), "<unknown>", -1);

		return;
	}
}

static int prepare_paths(void) {
	char *home;
	struct passwd *pw;
	char redapid_dirname[1024];
	struct stat st;

	if (getuid() == 0) {
		return 0;
	}

	home = getenv("HOME");

	if (home == NULL || *home == '\0') {
		pw = getpwuid(getuid());

		if (pw == NULL) {
			fprintf(stderr, "Could not determine home directory: %s (%d)\n",
			        get_errno_name(errno), errno);

			return -1;
		}

		home = pw->pw_dir;
	}

	if (robust_snprintf(redapid_dirname, sizeof(redapid_dirname),
	                    "%s/.redapid", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid directory name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_config_filename, sizeof(_config_filename),
	                    "%s/.redapid/redapid.conf", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid.conf file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_pid_filename, sizeof(_pid_filename),
	                    "%s/.redapid/redapid.pid", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid.pid file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_brickd_socket_filename, sizeof(_brickd_socket_filename),
	                    "%s/.redapid/redapid-brickd.socket", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid-brickd.socket file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_cron_socket_filename, sizeof(_cron_socket_filename),
	                    "%s/.redapid/redapid-cron.socket", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid-cron.socket file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_log_filename, sizeof(_log_filename),
	                    "%s/.redapid/redapid.log", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid.log file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (mkdir(redapid_dirname, 0755) < 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "Could not create directory '%s': %s (%d)\n",
			        redapid_dirname, get_errno_name(errno), errno);

			return -1;
		}

		if (stat(redapid_dirname, &st) < 0) {
			fprintf(stderr, "Could not get information for '%s': %s (%d)\n",
			        redapid_dirname, get_errno_name(errno), errno);

			return -1;
		}

		if (!S_ISDIR(st.st_mode)) {
			fprintf(stderr, "Expecting '%s' to be a directory\n", redapid_dirname);

			return -1;
		}
	}

	return 0;
}

static void print_usage(void) {
	printf("Usage:\n"
	       "  redapid [--help|--version|--check-config|--daemon] [--debug [<filter>]]\n"
	       "\n"
	       "Options:\n"
	       "  --help              Show this help\n"
	       "  --version           Show version number\n"
	       "  --check-config      Check config file for errors\n"
	       "  --daemon            Run as daemon and write PID and log file\n"
	       "  --debug [<filter>]  Set log level to debug and apply optional filter\n");
}

static void handle_sighup(void) {
	IO *output;

	log_get_output(&output, NULL);

	if (output != &_log_file.base) {
		return;
	}

	log_set_output(&log_stderr_output, NULL);

	file_destroy(&_log_file);

	if (file_create(&_log_file, _log_filename,
	                O_CREAT | O_WRONLY | O_APPEND, 0644) < 0) {
		log_error("Could not reopen log file '%s': %s (%d)",
		          _log_filename, get_errno_name(errno), errno);

		return;
	}

	log_set_output(&_log_file.base, NULL);

	log_info("Reopened log file '%s'", _log_filename);
}

int main(int argc, char **argv) {
	int exit_code = EXIT_FAILURE;
	int i;
	bool help = false;
	bool version = false;
	bool check_config = false;
	bool daemon = false;
	const char *debug_filter = NULL;
	int pid_fd = -1;

	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0) {
			help = true;
		} else if (strcmp(argv[i], "--version") == 0) {
			version = true;
		} else if (strcmp(argv[i], "--check-config") == 0) {
			check_config = true;
		} else if (strcmp(argv[i], "--daemon") == 0) {
			daemon = true;
		} else if (strcmp(argv[i], "--debug") == 0) {
			if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0) {
				debug_filter = argv[++i];
			} else {
				debug_filter = "";
			}
		} else {
			fprintf(stderr, "Unknown option '%s'\n\n", argv[i]);
			print_usage();

			return EXIT_FAILURE;
		}
	}

	if (help) {
		print_usage();

		return EXIT_SUCCESS;
	}

	if (version) {
		printf("%s\n", VERSION_STRING);

		return EXIT_SUCCESS;
	}

	read_image_version();

	_x11_enabled = access("/etc/tf_x11_enabled", F_OK) == 0;

	if (prepare_paths() < 0) {
		return EXIT_FAILURE;
	}

	if (check_config) {
		return config_check(_config_filename) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	config_init(_config_filename);

	if (config_has_error()) {
		fprintf(stderr, "Error(s) occurred while reading config file '%s'",
		        _config_filename);

		goto error_config;
	}

	log_init();

	if (daemon) {
		pid_fd = daemon_start(_log_filename, &_log_file, _pid_filename, 1);
	} else {
		pid_fd = pid_file_acquire(_pid_filename, getpid());

		if (pid_fd == PID_FILE_ALREADY_ACQUIRED) {
			fprintf(stderr, "Already running according to '%s'\n", _pid_filename);
		}
	}

	if (pid_fd < 0) {
		goto error_pid_file;
	}

	log_info("RED Brick API Daemon %s started (pid: %u, daemonized: %d, X11: %d) on %s image",
	         VERSION_STRING, getpid(), daemon ? 1 : 0, _x11_enabled ? 1 : 0, _image_version);

	if (debug_filter != NULL) {
		log_enable_debug_override(debug_filter);
	}

	if (config_has_warning()) {
		log_warn("Warning(s) in config file '%s', run with --check-config option for details",
		         _config_filename);
	}

	if (event_init() < 0) {
		goto error_event;
	}

	if (signal_init(handle_sighup, NULL) < 0) {
		goto error_signal;
	}

	if (process_monitor_init() < 0) {
		goto error_process_monitor;
	}

	if (cron_init() < 0) {
		goto error_cron;
	}

	if (inventory_init() < 0) {
		goto error_inventory;
	}

	if (api_init() < 0) {
		goto error_api;
	}

	if (network_init(_brickd_socket_filename, _cron_socket_filename) < 0) {
		goto error_network;
	}

	// FIXME: delay program start to avoid starting programs before async
	//        stack discovery is complete in brick

	if (inventory_load_programs() < 0) {
		goto error_load_programs;
	}

	if (event_run(network_cleanup_brickd_and_socats) < 0) {
		goto error_run;
	}

	exit_code = EXIT_SUCCESS;

error_run:
	inventory_unload_programs();

error_load_programs:
	network_exit();

error_network:
	api_exit();

error_api:
	inventory_exit();

error_inventory:
	cron_exit();

error_cron:
	process_monitor_exit();

error_process_monitor:
	signal_exit();

error_signal:
	event_exit();

error_event:
	log_info("RED Brick API Daemon %s stopped", VERSION_STRING);

error_pid_file:
	if (pid_fd >= 0) {
		pid_file_release(_pid_filename, pid_fd);
	}

	log_exit();

error_config:
	config_exit();

	return exit_code;
}
