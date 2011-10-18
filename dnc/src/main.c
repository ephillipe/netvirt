/*
 * main.c: Initialise DNC subsystems
 *
 * Copyright (C) 2010 Nicolas Bouliane
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 */

#include <unistd.h>

#include <dnds/event.h>
#include <dnds/journal.h>
#include <dnds/net.h>
#include <dnds/netbus.h>
#include <dnds/options.h>
#include <dnds/xsched.h>

#include "dnc.h"
#include "certificates.h"

#ifndef CONFIG_FILE
# define CONFIG_FILE "/usr/local/etc/dnds/dnc.conf"
#endif

char *server_address = NULL;
char *server_port = NULL;

struct options opts[] = {

	{ "server_address",	&server_address,	OPT_STR | OPT_MAN },
	{ "server_port",	&server_port,		OPT_STR | OPT_MAN },

	{ NULL }
};
int main(int argc, char *argv[])
{
	int cert_num;

	if (getuid() != 0) {
		JOURNAL_ERR("dnc]> you must be root");
		return -1;
	}

	if (option_parse(opts, CONFIG_FILE)) {
		JOURNAL_ERR("dnc]> option_parse() failed :: %s:%i", __FILE__, __LINE__);
		_exit(EXIT_ERR);
	}

	option_dump(opts);

	if (event_init()) {
		JOURNAL_ERR("dnc]> event_init() failed :: %s:%i", __FILE__, __LINE__);
		_exit(EXIT_ERR);
	}

	if (scheduler_init()) {
		JOURNAL_ERR("dnc]> scheduler_init() failed :: %s:%i", __FILE__, __LINE__);
		_exit(EXIT_ERR);
	}

	if (netbus_init()) {
		JOURNAL_ERR("dnc]> netbus_init() failed :: %s:%i", __FILE__, __LINE__);
		_exit(EXIT_ERR);
	}

	krypt_init();

	JOURNAL_INFO("dnc]> connecting...");

	if (dnc_init(server_address, server_port)) {
		JOURNAL_ERR("dnc]> dnc_init() failed :: %s:%i", __FILE__, __LINE__);
		_exit(EXIT_ERR);
	}

	scheduler();

	return 0;
}
