/**
 * collectd - src/battery.c
 * Copyright (C) 2006  Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Florian octo Forster <octo at verplant.org>
 **/

#include "collectd.h"
#include "common.h"
#include "plugin.h"

#define MODULE_NAME "battery"
#define BUFSIZE 512

#if defined(KERNEL_LINUX)
# define BATTERY_HAVE_READ 1
#else
# define BATTERY_HAVE_READ 0
#endif

static char *battery_current_file = "battery-%s/current.rrd";
static char *battery_voltage_file = "battery-%s/voltage.rrd";
static char *battery_charge_file  = "battery-%s/charge.rrd";

static char *ds_def_current[] =
{
	"DS:current:GAUGE:25:0:U",
	NULL
};
static int ds_num_current = 1;

static char *ds_def_voltage[] =
{
	"DS:voltage:GAUGE:25:0:U",
	NULL
};
static int ds_num_voltage = 1;

static char *ds_def_charge[] =
{
	"DS:charge:GAUGE:25:0:U",
	NULL
};
static int ds_num_charge = 1;

static int   battery_pmu_num = 0;
static char *battery_pmu_file = "/proc/pmu/battery_%i";

static void battery_init (void)
{
#if BATTERY_HAVE_READ
	int len;
	char filename[BUFSIZE];

	for (battery_pmu_num = 0; ; battery_pmu_num++)
	{
		len = snprintf (filename, BUFSIZE, battery_pmu_file, battery_pmu_num);

		if ((len >= BUFSIZE) || (len < 0))
			break;

		if (access (filename, R_OK))
			break;
	}
#endif

	return;
}

static void battery_current_write (char *host, char *inst, char *val)
{
	rrd_update_file (host, battery_current_file, val,
			ds_def_current, ds_num_current);
}

static void battery_voltage_write (char *host, char *inst, char *val)
{
	rrd_update_file (host, battery_voltage_file, val,
			ds_def_voltage, ds_num_voltage);
}

static void battery_charge_write (char *host, char *inst, char *val)
{
	rrd_update_file (host, battery_charge_file, val,
			ds_def_charge, ds_num_charge);
}

#if BATTERY_HAVE_READ
static void battery_submit (int batnum, double current, double voltage, double charge)
{
	int len;
	char buffer[BUFSIZE];
	char batnum_str[BUFSIZE];

	len = snprintf (batnum_str, BUFSIZE, "%i", batnum);
	if ((len >= BUFSIZE) || (len < 0))
		return;

	if (current > 0.0)
	{
		len = snprintf (buffer, BUFSIZE, "N:%.3f", current);

		if ((len > 0) && (len < BUFSIZE))
			plugin_submit ("battery_current", batnum_str, buffer);
	}

	if (voltage > 0.0)
	{
		len = snprintf (buffer, BUFSIZE, "N:%.3f", voltage);

		if ((len > 0) && (len < BUFSIZE))
			plugin_submit ("battery_voltage", batnum_str, buffer);
	}

	if (charge > 0.0)
	{
		len = snprintf (buffer, BUFSIZE, "N:%.3f", charge);

		if ((len > 0) && (len < BUFSIZE))
			plugin_submit ("battery_charge", batnum_str, buffer);
	}
}

static void battery_read (void)
{
#ifdef KERNEL_LINUX
	FILE *fh;
	char buffer[BUFSIZE];
	char filename[BUFSIZE];
	
	char *fields[8];
	int numfields;

	int i;
	int len;

	for (i = 0; i < battery_pmu_num; i++)
	{
		double current = 0.0;
		double voltage = 0.0;
		double charge  = 0.0;

		len = snprintf (filename, BUFSIZE, battery_pmu_file, i);

		if ((len >= BUFSIZE) || (len < 0))
			continue;

		if ((fh = fopen (filename, "r")) == NULL)
			continue;

		while (fgets (buffer, BUFSIZE, fh) != NULL)
		{
			numfields = strsplit (buffer, fields, 8);

			if (numfields < 3)
				continue;

			if (strcmp ("current", fields[0]) == 0)
				current = atof (fields[2]) / 1000;
			else if (strcmp ("voltage", fields[0]) == 0)
				voltage = atof (fields[2]) / 1000;
			else if (strcmp ("charge", fields[0]) == 0)
				charge = atof (fields[2]) / 1000;
		}

		if ((current != 0.0) || (voltage != 0.0) || (charge != 0.0))
			battery_submit (i, current, voltage, charge);

		fclose (fh);
		fh = NULL;
	}
#endif /* KERNEL_LINUX */
}
#else
# define battery_read NULL
#endif /* BATTERY_HAVE_READ */

void module_register (void)
{
	plugin_register (MODULE_NAME, battery_init, battery_read, NULL);
	plugin_register ("battery_current", NULL, NULL, battery_current_write);
	plugin_register ("battery_voltage", NULL, NULL, battery_voltage_write);
	plugin_register ("battery_charge",  NULL, NULL, battery_charge_write);
}

#undef BUFSIZE
#undef MODULE_NAME
