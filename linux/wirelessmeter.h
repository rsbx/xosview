//
//  Copyright (c) 1997 by Mike Romberg ( romberg@fsl.noaa.gov )
//  Copyright (c) 2009, 2010, 2013 by Raymond S Brand <rsbx@acm.org>
//
//  This file may be distributed under terms of the GPL

#ifndef _WIRELESSMETER_H_
#define _WIRELESSMETER_H_

#include "fieldmetergraph.h"
#include "MeterMaker.h"

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/wireless.h>


class WirelessMeter : public FieldMeterGraph
	{
	public:
		WirelessMeter(XOSView *parent, int ID = 1, const char *wlID = "WL");
		~WirelessMeter(void);

		static void makeMeters(XOSView *xosview, MeterMaker *mmake);
		const char *name(void) const
			{
			 return "WirelessMeter";
			}
		void checkevent(void);
		void checkResources(void);
		static unsigned int countdevices(void);
		static const char *wirelessStr(int num);

	protected:

	private:
		int		_number;

		struct iwreq	iwrq;
		int		val_max;		// range max
		int		val_good;		// >= is good
		int		val_fair;		// < is poor
		int		val_min;		// range min
		int		hint_status;		// val_max from driver?

		// Used if the driver does not provide range info
		int		range_granularity;
		float		range_ratio;


		void update_stats(void);
		void check_hints(struct iw_quality *qual_info);
	};


#endif
