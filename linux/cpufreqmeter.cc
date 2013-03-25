//
//  Copyright (c) 2010, 2013 Raymond S Brand
//
//  This file may be distributed under terms of (any) BSD license or equivalent.
//

#include "cpufreqmeter.h"
#include "xosview.h"
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>


#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define GEN_TOGGLE(G) ((G) ? 0 : 1)
#define TIME_DELTA(GEN, FREQ) (MAX(0, (double)stats[GEN].time[FREQ]-(double)stats[GEN_TOGGLE(GEN)].time[FREQ]))


#define SYS_CPU		"/sys/devices/system/cpu"
#define CPUFREQ_STATS	"cpufreq/stats/time_in_state"
#define CPUFREQ_MAX	"cpufreq/cpuinfo_max_freq"
#define CPUFREQ_MIN	"cpufreq/cpuinfo_min_freq"


struct cpufreq_info
	{
	unsigned int	cpuid;
	};


static struct cpufreq_info *cpufreq_array = NULL;
static int cpufreq_count = -1;


static const char *cpuIdStr(unsigned int num);


void CPUFreqMeter::makeMeters(XOSView *xosview, MeterMaker *mmake)
	{
	if (xosview->isResourceTrue("linux.cpuFreq.enable"))
		{
		unsigned int count = CPUFreqMeter::countCpuFreqDevices();
		for (unsigned int i=0; i<count; i++)
			{
			mmake->push(new CPUFreqMeter(xosview, i));
			}
		}
	}


CPUFreqMeter::CPUFreqMeter(XOSView *parent, unsigned int freqID)
		: FieldMeterGraph(parent, 5, cpuIdStr(freqID), "MHz Low/LoMid/HiMid/High/Idle", 1, 1, 0)
	{
	unsigned int i;

	cpuID = cpufreq_array[freqID].cpuid;
	configured = false;
	for (i=0; i<2; i++)
		{
		stats[i].valid = false;
		stats[i].index = NULL;
		stats[i].freq = NULL;
		stats[i].time = NULL;
		}
	}


CPUFreqMeter::~CPUFreqMeter(void)
	{
	}


static char *vsformat(char *buffer, size_t size, const char *format, ...)
	{
	int r;
	va_list args;

	va_start(args, format);
	r = vsnprintf(buffer, size, format, args);
	va_end(args);

	if (r < 0 || r >= (int)size)
		{
		std::cerr << "Format string overflow: \"" << format << "\"." << std::endl;
		return NULL;
		}

	return buffer;
	}


void CPUFreqMeter::checkResources(void)
	{
	char *p0;
	char *p1;
	char *p2;
	long l;
	char buffer[1024];
#define RESOURCE_SEP " \t,;"

//	FieldMeterGraph::checkResources(vsformat(buffer, 1024, "linux.cpuFreq.%u", cpuID));
	FieldMeterGraph::checkResources();

	priority_ = atoi(parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.priority", cpuID)));
	dodecay_ = parent_->isResourceTrue(vsformat(buffer, 1024, "linux.cpuFreq.%u.decay", cpuID));
	useGraph_ = parent_->isResourceTrue(vsformat(buffer, 1024, "linux.cpuFreq.%u.graph", cpuID));
	SetUsedFormat (parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.usedFormat", cpuID)));

	setfieldcolor(3, parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.color.high", cpuID)));
	setfieldcolor(2, parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.color.midHigh", cpuID)));
	setfieldcolor(1, parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.color.midLow", cpuID)));
	setfieldcolor(0, parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.color.low", cpuID)));

	setfieldcolor(4, parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.color.idle", cpuID)));

	if (!(p0 = p1 = strdup(parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.scale.max", cpuID)))))
		{
		std::cerr << "strdup failed" << std::endl;
		exit(1);
		}
	if ((p2 = strchr(p1, '!')))
		{
		*p2 = '\0';
		}
	p1 = strtok_r(p1, RESOURCE_SEP, &p2);
	if (!strcasecmp(p1, "auto"))
		{
		scale_max_freq = 0;
		}
	else
		{
		if (!isdigit(*p1) || (l = strtoul(p1, &p2, 10), p1 == p2))
			{
			std::cerr << "Invalid value for \"" << buffer << "\" resource." << std::endl;
			exit(1);
			}
		scale_max_freq = l*1000+1;
		}
	free(p0);

	if (!(p0 = p1 = strdup(parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.scale.min", cpuID)))))
		{
		std::cerr << "strdup failed" << std::endl;
		exit(1);
		}
	if ((p2 = strchr(p1, '!')))
		{
		*p2 = '\0';
		}
	p1 = strtok_r(p1, RESOURCE_SEP, &p2);
	if (!strcasecmp(p1, "auto"))
		{
		scale_min_freq = 0;
		}
	else
		{
		if (!isdigit(*p1) || (l = strtoul(p1, &p2, 10), p1 == p2))
			{
			std::cerr << "Invalid value for \"" << buffer << "\" resource." << std::endl;
			exit(1);
			}
		scale_min_freq = l*1000+1;
		}
	free(p0);

	rangeThresholds[3] = atof(parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.scale.threshold.high", cpuID)));
	rangeThresholds[2] = atof(parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.scale.threshold.midHigh", cpuID)));
	rangeThresholds[1] = atof(parent_->getResource(vsformat(buffer, 1024, "linux.cpuFreq.%u.scale.threshold.midLow", cpuID)));
	rangeThresholds[0] = 0.0;

	for (int i=1; i<=3; i++)
		{
		rangeThresholds[i] = MAX(0.0, rangeThresholds[i]);
		}
	}


bool CPUFreqMeter::configure(void)
	{
	std::ifstream freqinfo;
	std::string line;
	static char filename[1024];
	unsigned int i, j, t, u;

	snprintf(filename, 1024, "%s/cpu%u/%s", SYS_CPU, cpuID, CPUFREQ_MAX);
	freqinfo.open(filename);
	if (!freqinfo)
		{
		return false;
		}
	freqinfo >> cpu_max_freq;
	freqinfo.close();
	freqinfo.clear();

	snprintf(filename, 1024, "%s/cpu%u/%s", SYS_CPU, cpuID, CPUFREQ_MIN);
	freqinfo.open(filename);
	if (!freqinfo)
		{
		return false;
		}

	freqinfo >> cpu_min_freq;
	freqinfo.close();
	freqinfo.clear();

	snprintf(filename, 1024, "%s/cpu%u/%s", SYS_CPU, cpuID, CPUFREQ_STATS);
	stats_file = strdup(filename);

	freq_count = 0;
	freqinfo.open(stats_file);
	if (!freqinfo)
		{
		return false;
		}

	while (freqinfo >> t >> u)
		{
		freq_count++;
		}
	freqinfo.close();
	freqinfo.clear();

	for (i=0; i<2; i++)
		{
		stats[i].valid = false;
		stats[i].index = new unsigned int[freq_count];
		stats[i].freq = new unsigned long[freq_count];
		stats[i].time = new unsigned long[freq_count];
		}

	stat_gen = 0;
	i = 0;
	freqinfo.open(stats_file);
	while (freqinfo >> stats[0].freq[i%freq_count] >> stats[0].time[i%freq_count])
		{
		i++;
		}
	freqinfo.close();
	freqinfo.clear();
	if (i != freq_count)
		{
		nostats();
		return false;
		}

	for (i=0; i<freq_count; i++)
		{
		for (j=numfields_-2; j>0; j--)
			{
			if (stats[0].freq[i] >= cpu_max_freq * rangeThresholds[j])
				{
				break;
				}
			}
		stats[0].index[i] = stats[1].index[i] = j;
		}

	stats[0].valid = true;

	configured = true;

	return configured;
	}


void CPUFreqMeter::nostats(void)
	{
	unsigned int i;

	freq_count = 0;

	configured = false;
	for (i=0; i<2; i++)
		{
		stats[i].valid = false;

		delete[] stats[i].index;
		delete[] stats[i].freq;
		delete[] stats[i].time;

		stats[i].index = NULL;
		stats[i].freq = NULL;
		stats[i].time = NULL;
		}

	return;
	}


void CPUFreqMeter::checkevent(void)
	{
	getfreqinfo();
	}


bool CPUFreqMeter::GetRawStats(void)
	{
	unsigned int i;
	std::ifstream freqinfo;

	if (!configured)
		{
		configure();
		}
	else
		{
		stat_gen = GEN_TOGGLE(stat_gen);

		i = 0;
		freqinfo.open(stats_file);
		while (freqinfo >> stats[stat_gen].freq[i%freq_count] >> stats[stat_gen].time[i%freq_count])
			{
			i++;
			}
		freqinfo.close();
		freqinfo.clear();
		if (i != freq_count)
			{
			nostats();
			}

		for (i=0; i<freq_count; i++)
			{
			if (stats[stat_gen].freq[i] != stats[GEN_TOGGLE(stat_gen)].freq[i])
				{
				nostats();
				break;
				}
			}
		stats[stat_gen].valid = configured;
		}

	return stats[0].valid && stats[1].valid;
	}


void CPUFreqMeter::getfreqinfo(void)
	{
	unsigned int i;
	double sample_interval;
	double scale_min, scale_max;
	double crt1, crt2, nrt1, nrt2;	// running totals, current & next

	if (!GetRawStats())
		{
		for (i=0; i<numfields_; i++)
			{
			fields_[i] = 0.0;
			}
		setUsed(0.0, 0.0);

		return;
		}

	sample_interval = 0;
	for (i=0; i<freq_count; i++)
		{
		sample_interval += TIME_DELTA(stat_gen, i);
		}

	if (sample_interval <= 0.0)
		{
		for (i=0; i<numfields_; i++)
			{
			fields_[i] = 0.0;
			}
		setUsed(0.0, 0.0);

		return;
		}

	for (i=0; i<numfields_-1; i++)
		{
		fields_[i] = 0.0;
		}

	for (i=0; i<freq_count; i++)
		{
		fields_[stats[stat_gen].index[i]]
				+= stats[stat_gen].freq[i] * TIME_DELTA(stat_gen, i) / sample_interval;
		}

	scale_min = (!scale_min_freq) ? cpu_min_freq : scale_min_freq-1;
	scale_max = (!scale_max_freq) ? cpu_max_freq : scale_max_freq-1;

	crt1 = 0.0;
	crt2 = scale_min;
	fields_[numfields_-1] = scale_max-scale_min;
	for (i=0; i<numfields_-1; i++)
		{
		nrt1 = crt1 + fields_[i];
		fields_[i] = MIN(fields_[i], MAX(0.0, nrt1-scale_min));

		nrt2 = crt2 + fields_[i];
		fields_[i] = MIN(fields_[i], MAX(0.0, scale_max-crt2));

		crt1 = nrt1;
		crt2 = nrt2;
		fields_[numfields_-1] -= fields_[i];
		}

	setUsed(crt1/1000, cpu_max_freq/1000);
	}


static const char *cpuIdStr(unsigned int num)
	{
	static char buffer[32];

	snprintf(buffer, 32, "CLK%u", cpufreq_array[num].cpuid);
	return buffer;
	}


static int has_cpufreq_stats(unsigned int id)
	{
	char		buffer[1024];

	snprintf(buffer, 1024, "%s/cpu%u/%s", SYS_CPU, id, CPUFREQ_STATS); buffer[1023] = '\0';
	if (access(buffer, R_OK))
		{
		return 0;
		}

	snprintf(buffer, 1024, "%s/cpu%u/%s", SYS_CPU, id, CPUFREQ_MAX); buffer[1023] = '\0';
	if (access(buffer, R_OK))
		{
		return 0;
		}

	snprintf(buffer, 1024, "%s/cpu%u/%s", SYS_CPU, id, CPUFREQ_MIN); buffer[1023] = '\0';
	if (access(buffer, R_OK))
		{
		return 0;
		}

	return 1;
	}


static int cmp_cpuID(const void *p1, const void *p2)
	{
	return (((struct cpufreq_info *)p1)->cpuid > ((struct cpufreq_info *)p2)->cpuid)
			? 1
			: (((struct cpufreq_info *)p1)->cpuid < ((struct cpufreq_info *)p2)->cpuid)
					? -1
					: 0
			;
	}


unsigned int CPUFreqMeter::countCpuFreqDevices(void)
	{
	DIR		*dir;
	char		*p;
	unsigned int	id;
	struct dirent	*dent;

	if (cpufreq_count >= 0)
		{
		return cpufreq_count;
		}

	cpufreq_count = 0;

	if ((dir = opendir(SYS_CPU)))
		{
		while ((dent = readdir(dir)))
			{
			if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")
					|| strncmp(dent->d_name, "cpu", 3)
					|| dent->d_name+4 == '\0'
					)
				{
				continue;
				}

			p = dent->d_name + 3;
			id = 0;
			while (isdigit(*p))
				{
				id = id*10 + *p - '0';
				p++;
				}

			if (*p || !has_cpufreq_stats(id))
				{
				continue;
				}

			if (!(cpufreq_array = (struct cpufreq_info *)realloc(
					cpufreq_array,
					(cpufreq_count+1)*sizeof(struct cpufreq_info)
					)))
				{
				perror("Realloc failed");
				exit(1);
				}

			cpufreq_array[cpufreq_count].cpuid = id;
			cpufreq_count++;
			}
		}
	closedir(dir);

	qsort(cpufreq_array, cpufreq_count, sizeof(struct cpufreq_info), cmp_cpuID);

	return cpufreq_count;
	}
