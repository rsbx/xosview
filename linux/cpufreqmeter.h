//
//  Copyright (c) 2010, 2013 Raymond S Brand
//
//  This file may be distributed under terms of (any) BSD license or equivalent.
//

#ifndef _CPUFREQMETER_H_
#define _CPUFREQMETER_H_

#include "fieldmetergraph.h"
#include "MeterMaker.h"


struct cpuFreq_FreqStats
	{
	bool		valid;
	unsigned int 	*index;
	unsigned long	*freq;
	unsigned long	*time;
	};


class CPUFreqMeter : public FieldMeterGraph
	{
	public:
		CPUFreqMeter(XOSView *parent, unsigned int freqID);
		~CPUFreqMeter(void);

		static void makeMeters(XOSView *xosview, MeterMaker *mmake);
		const char *name(void) const
			{
			return "CPUFreqMeter";
			}
		void checkevent(void);
		void checkResources(void);
		static unsigned int countCpuFreqDevices(void);

	protected:

	private:
		unsigned int cpuID;

		const char *stats_file;

		float rangeThresholds[4];
		unsigned long scale_max_freq;
		unsigned long scale_min_freq;

		unsigned long cpu_max_freq;
		unsigned long cpu_min_freq;

		bool configured;
		unsigned int freq_count;
		unsigned int stat_gen;
		struct cpuFreq_FreqStats stats[2];


		void getfreqinfo(void);
		bool configure(void);
		void nostats(void);
		bool GetRawStats(void);
	};

#endif
