//
//  Copyright (c) 1994, 1995, 2002, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
//
//  This file may be distributed under terms of the GPL
//

#include "MeterMaker.h"
#include "xosview.h"

#include "cpumeter.h"
#include "cpufreqmeter.h"
#include "memmeter.h"
#include "swapmeter.h"
#include "pagemeter.h"
#include "netmeter.h"
#include "intmeter.h"
#include "serialmeter.h"
#include "loadmeter.h"
#include "btrymeter.h"
#include "wirelessmeter.h"
#include <fstream>
#include "diskmeter.h"
#include "raidmeter.h"
#include "lmstemp.h"
#include "acpitemp.h"
#include "coretemp.h"
#include "nfsmeter.h"

#include <stdlib.h>

#include <sstream>
#include <iomanip>


#define RESOURCE_SEP	" \t,;"


struct metermaker
	{
	const char *metric;
	bool made;
	void (*maker)(XOSView *xosview, MeterMaker *mmake);
	};


struct metermaker makertable[] =
	{
	{"load",	false,	&LoadMeter::makeMeters},
	{"cpuFreq",	false,	&CPUFreqMeter::makeMeters},
	{"cpu",		false,	&CPUMeter::makeMeters},
	{"mem",		false,	&MemMeter::makeMeters},
	{"disk",	false,	&DiskMeter::makeMeters},
	{"wireless",	false,	&WirelessMeter::makeMeters},
	{"RAID",	false,	&RAIDMeter::makeMeters},
	{"swap",	false,	&SwapMeter::makeMeters},
	{"page",	false,	&PageMeter::makeMeters},
	{"net",		false,	&NetMeter::makeMeters},
	{"NFSDStat",	false,	&NFSDStats::makeMeters},
	{"NFSStat",	false,	&NFSStats::makeMeters},
	{"serial",	false,	&SerialMeter::makeMeters},
	{"int",		false,	&IntMeter::makeMeters},
	{"battery",	false,	&BtryMeter::makeMeters},
	{"coretemp",	false,	&CoreTemp::makeMeters},
	{"lmstemp",	false,	&LmsTemp::makeMeters},
	{"acpitemp",	false,	&ACPITemp::makeMeters}
	};


MeterMaker::MeterMaker(XOSView *xos)
	{
	xosview = xos;
	}


void MeterMaker::makeMeters(void)
	{
	unsigned int i;
	char *p0;
	char *p1;
	char *p2;


	if (!(p0 = p1 = strdup(xosview->getResource("linux.metrics"))))
		{
		std::cerr << "strdup failed" << std::endl;
		exit(1);
		}
	if ((p2 = strchr(p1, '!')))
		{
		*p2 = '\0';
		}
	p1 = strtok_r(p1, RESOURCE_SEP, &p2);
	while (p1)
		{
		for (i=0; i<sizeof(makertable)/sizeof(struct metermaker); i++)
			{
			if (makertable[i].made || strcmp(p1, makertable[i].metric))
				{
				continue;
				}
			(makertable[i].maker)(xosview, this);
			makertable[i].made = true;
			break;
			}
		p1 = strtok_r(NULL, RESOURCE_SEP, &p2);
		}
	free(p0);
	}
