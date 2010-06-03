/*
 * Media controller test application
 *
 * Copyright (C) 2010 Ideas on board SPRL <laurent.pinchart@ideasonboard.com>
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
 */

#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include "options.h"

#define MEDIA_DEVNAME_DEFAULT		"/dev/media0"

struct media_options media_opts = {
	.devname = MEDIA_DEVNAME_DEFAULT,
};

static void usage(const char *argv0)
{
	printf("%s [options] device\n", argv0);
	printf("-d, --device dev	Media device name (default: %s)\n", MEDIA_DEVNAME_DEFAULT);
	printf("-e, --entity name	Print the device name associated with the given entity\n");
	printf("-f, --formats		Comma-separated list of formats to setup\n");
	printf("-h, --help		Show this help screen\n");
	printf("-i, --interactive	Modify links interactively\n");
	printf("-l, --links		Comma-separated list of links descriptors to setup\n");
	printf("-p, --print-topology	Print the device topology (implies -v)\n");
	printf("    --print-dot		Print the device topology as a dot graph (implies -v)\n");
	printf("-r, --reset		Reset all links to inactive\n");
	printf("-v, --verbose		Be verbose\n");
}

#define OPT_PRINT_DOT	256

static struct option opts[] = {
	{"device", 1, 0, 'd'},
	{"entity", 1, 0, 'e'},
	{"formats", 1, 0, 'f'},
	{"help", 0, 0, 'h'},
	{"interactive", 0, 0, 'i'},
	{"links", 1, 0, 'l'},
	{"print-dot", 0, 0, OPT_PRINT_DOT},
	{"print-topology", 0, 0, 'p'},
	{"reset", 0, 0, 'r'},
	{"verbose", 0, 0, 'v'},
};

int parse_cmdline(int argc, char **argv)
{
	int opt;

	if (argc == 1) {
		usage(argv[0]);
		return 1;
	}

	/* parse options */
	while ((opt = getopt_long(argc, argv, "d:e:f:hil:prv", opts, NULL)) != -1) {
		switch (opt) {
		case 'd':
			media_opts.devname = optarg;
			break;

		case 'e':
			media_opts.entity = optarg;
			break;

		case 'f':
			media_opts.formats = optarg;
			break;

		case 'h':
			usage(argv[0]);
			exit(0);

		case 'i':
			media_opts.interactive = 1;
			break;

		case 'l':
			media_opts.links = optarg;
			break;

		case 'p':
			media_opts.print = 1;
			media_opts.verbose = 1;
			break;

		case 'r':
			media_opts.reset = 1;
			break;

		case 'v':
			media_opts.verbose = 1;
			break;

		case OPT_PRINT_DOT:
			media_opts.print_dot = 1;
			break;

		default:
			printf("Invalid option -%c\n", opt);
			printf("Run %s -h for help.\n", argv[0]);
			return 1;
		}
	}

	return 0;
}

