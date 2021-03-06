//
//  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
//  Copyright (c) 1995, 1996, 1997-2002 by Brian Grayson (bgrayson@netbsd.org)
//
//  This file was written by Brian Grayson for the NetBSD and xosview
//    projects.
//  This file may be distributed under terms of the GPL or of the BSD
//    license, whichever you choose.  The full license notices are
//    contained in the files COPYING.GPL and COPYING.BSD, which you
//    should have received.  If not, contact one of the xosview
//    authors for a copy.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "kernel.h"
#include "MeterMaker.h"
#include "xosview.h"
#ifdef HAVE_BATTERY_METER
#include "btrymeter.h"
#endif
#include "cpumeter.h"
#include "memmeter.h"
#include "swapmeter.h"
#include "netmeter.h"
#include "loadmeter.h"
#include "diskmeter.h"
#include "pagemeter.h"
#include "intmeter.h"
#include "intratemeter.h"
#include "coretemp.h"
#include "sensor.h"
//  This one is not yet supported under *BSD.
//#include "serialmeter.h"

MeterMaker::MeterMaker(XOSView *xos) {
	_xos = xos;
}

void MeterMaker::makeMeters(void) {
	// Standard meters (usually added, but users could turn them off)
	if ( _xos->isResourceTrue("load") )
		push(new LoadMeter(_xos));

	if ( _xos->isResourceTrue("cpu") )
		push(new CPUMeter(_xos));

	if ( _xos->isResourceTrue("mem") )
		push(new MemMeter(_xos));

	if ( _xos->isResourceTrue("swap") )
		push(new SwapMeter(_xos));

	if ( _xos->isResourceTrue("page") )
		push(new PageMeter(_xos, atof(_xos->getResource("pageBandwidth"))));

	if ( _xos->isResourceTrue("net") )
		push(new NetMeter(_xos, atof(_xos->getResource("netBandwidth"))));

	if ( _xos->isResourceTrue("disk") )
		push(new DiskMeter(_xos, atof(_xos->getResource("diskBandwidth"))));

	if ( _xos->isResourceTrue("interrupts") )
		push(new IntMeter(_xos));

	if ( _xos->isResourceTrue("irqrate") )
		push(new IrqRateMeter(_xos));

	if ( _xos->isResourceTrue("coretemp") ) {
		char caption[25];
		snprintf(caption, 25, "TEMPERATURE (C)/%s",
		         _xos->getResourceOrUseDefault( "coretempHighest", "100" ) );
		const char *displayType = _xos->getResourceOrUseDefault("coretempDisplayType", "separate");
		if (strncmp(displayType, "separate", 1) == 0) {
			char name[5];
			for (int i = 0; i < BSDCountCpus(); i++) {
				snprintf(name, 5, "CPU%d", i);
				push(new CoreTemp(_xos, name, caption, i));
			}
		}
		else if (strncmp(displayType, "average", 1) == 0)
			push(new CoreTemp(_xos, "CPU", caption, -1));
		else if (strncmp(displayType, "maximum", 1) == 0)
			push(new CoreTemp(_xos, "CPU", caption, -2));
		else {
			std::cerr << "Unknown value of coretempDisplayType: " << displayType << std::endl;
			_xos->done(1);
		}
	}

	if ( _xos->isResourceTrue("bsdsensor") ) {
		char caption[25], label[5];
		for (int i = 1 ; ; i++) {
			char s[20];
			snprintf(s, 20, "bsdsensorHigh%d", i);
			const char *high = _xos->getResourceOrUseDefault(s, "100");
			snprintf(caption, 25, "ACT/HIGH/%s", high);
			snprintf(s, 20, "bsdsensor%d", i);
			const char *res = _xos->getResourceOrUseDefault(s, NULL);
			if (!res || !*res)
				break;
			snprintf(s, 20, "bsdsensorLabel%d", i);
			snprintf(label, 5, "SEN%d", i);
			const char *lab = _xos->getResourceOrUseDefault(s, label);
			push(new BSDSensor(_xos, res, high, lab, caption, i));
		}
	}


#ifdef HAVE_BATTERY_METER
	//  NOTE:  None of the recent *BSDs supports the Battery Meter any longer.
	//  This one is done in its own file, not kernel.cc
	if ( _xos->isResourceTrue("battery") )
		push(new BtryMeter(_xos));
#endif

	//  The serial meters are not yet available for the BSDs.  BCG
}
