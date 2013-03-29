//
//  Copyright (c) 2013 by Raymond S Brand
//
//  This file may be distributed under the terms of (any) BSD license or equivalent.
//
//  Rewritten by Raymond S Brand with help from wireless-tools and wavemon.
//

#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "xosview.h"
#include "wirelessmeter.h"


#define ICEIL(x) -((int)(-x))
#define wMIN(a,b) (((a)<(b))?(a):(b))
#define wMAX(a,b) (((a)>(b))?(a):(b))

#define MIN_WEXT 16
#define MAX_WEXT 22

#if !(MIN_WEXT <= WIRELESS_EXT) || !(WIRELESS_EXT <= MAX_WEXT)
#error Code must be checked for wireless extensions compatibility.
#endif


static const char SYS_NET[] =		"/sys/class/net";
static const char PROC_NET_DEV[] =	"/proc/net/dev";

struct wif_info
	{
	const char	*ifname;
	};

static int fd = -1;	// NOT thread safe!!
static int wif_count = -1;
static struct wif_info *wif_array = NULL;


WirelessMeter::WirelessMeter(XOSView *parent, int ID, const char *wlID)
		: FieldMeterGraph(parent, 4, wlID, "Poor/Fair/Good/Free", 1, 1, 0), _number(ID)
	{
	if (wif_count < 0)
		{
		(void)countdevices();
		}

	if (!(0 <= ID) && !(ID < wif_count))
		{
		std::cerr <<"Attempt to access device " << ID << " of " << wif_count-1 << std::endl;
		exit(1);
		}

	memset(&iwrq, 0, sizeof(struct iwreq));
	strncpy(iwrq.ifr_name, wif_array[ID].ifname, IFNAMSIZ);

	// Range values will be initialized when the stats are accessed the first time
	hint_status = -1;
	}


WirelessMeter::~WirelessMeter(void)
	{
	}


void WirelessMeter::checkResources(void)
	{
	unsigned int i;
	static char buffer[256];
	static const char *colorlist[] = { "Poor", "Fair", "Good", "Idle" };

	FieldMeterGraph::checkResources();

	priority_ = atoi(parent_->getResource("wirelessPriority"));
	dodecay_ = parent_->isResourceTrue("wirelessDecay");
	SetUsedFormat(parent_->getResource("wirelessUsedFormat"));
	useGraph_ = parent_->isResourceTrue("wirelessGraph");

	for (i=0; i<sizeof(colorlist)/sizeof(const char *); i++)
		{
		snprintf(buffer, 255, "wirelessColors.%s.%s", iwrq.ifr_name, colorlist[i]);
		buffer[255] = '\0';
		setfieldcolor(i, parent_->allocColor(parent_->getResource(buffer)));
		}

	snprintf(buffer, 255, "wirelessRange.%s.Granularity", iwrq.ifr_name);
	buffer[255] = '\0';
	range_granularity = atoi(parent_->getResource(buffer));
	snprintf(buffer, 255, "wirelessRange.%s.Ratio", iwrq.ifr_name);
	buffer[255] = '\0';
	range_ratio = atof(parent_->getResource(buffer));

	if (!(0 < range_granularity) && !(range_granularity < 255))
		{
		std::cerr << "Warning: Invalid Granularity resource for device " << iwrq.ifr_name << " ignored.\n";
		exit(1);
		}
	if (!(0.0 < range_ratio) && !(range_ratio < 1.0))
		{
		std::cerr << "Warning: Invalid Ratio resource for device " << iwrq.ifr_name << " ignored.\n";
		exit(1);
		}
	}


void WirelessMeter::checkevent(void)
	{
	update_stats();
	}


#define WIF_STAT_QUAL_VALID	0x01
#define WIF_STAT_SIGNAL_VALID	0x02
#define WIF_STAT_NOISE_VALID	0x04
#define WIF_STAT_VALID_MASK	0x03
#define WIF_STAT_UNITS_DBM	0x80

#define WIF_IW_QUAL_ENCODED	(IW_QUAL_DBM | IW_QUAL_RCPI)

#define WIF_BOGUS_DBM -127	// from wavemon

#define IMID(hi, lo, ratio) wMIN((hi)-1, wMAX((lo), (lo) + ICEIL((float)((hi)-(lo))*(ratio))))

struct wif_stats
	{
	int flags;
	int quality;
	int signal;
	int noise;
	};


static int decode_u8(int flags, __u8 u8)
	{
	if (flags & IW_QUAL_DBM)
		{
		return (u8 >= 64) ? (int)u8 - 0x100 : u8;
		}
	else if (flags & IW_QUAL_RCPI)
		{
		return (u8 / 2.0) - 110.0;
		}

	return u8;
	}


static void decode_wifstats(struct iw_quality *qual, struct wif_stats *stats)
	{
	int val;

	memset(stats, 0, sizeof(*stats));

	// ALWAYS prefer quality if it's available
	if (!(qual->updated & IW_QUAL_QUAL_INVALID))
		{
		stats->quality = qual->qual;
		stats->flags |= WIF_STAT_QUAL_VALID;
		}
	else if (!(qual->updated & IW_QUAL_LEVEL_INVALID))
		{
		val = decode_u8(qual->updated, qual->level);
		if (!(qual->updated & WIF_IW_QUAL_ENCODED)
				|| val > WIF_BOGUS_DBM)
			{
			stats->signal = val;
			stats->flags |= WIF_STAT_SIGNAL_VALID;
			stats->flags |= (qual->updated & WIF_IW_QUAL_ENCODED)
					? WIF_STAT_UNITS_DBM
					: 0;
			}
		}
	}


void WirelessMeter::check_hints(struct iw_quality *qual_info)
	{
	struct wif_stats wifstats;
	struct wif_stats maxstats;
	struct wif_stats avgstats;
	struct iw_range range_info;

	decode_wifstats(qual_info, &wifstats);

	memset(&range_info, 0, sizeof(range_info));
	iwrq.u.data.pointer = (caddr_t)&range_info;
	iwrq.u.data.length  = sizeof(range_info);
	iwrq.u.data.flags   = 0;
	if (!(ioctl(fd, SIOCGIWRANGE, &iwrq) < 0)
			&& iwrq.u.data.length >= 300	// Magic constant from wireless-tools
			&& range_info.we_version_compiled >= MIN_WEXT
			&& range_info.we_version_source >= MIN_WEXT
			&& range_info.we_version_compiled <= MAX_WEXT
			&& range_info.we_version_source <= MAX_WEXT
			)
		{
		decode_wifstats(&range_info.max_qual, &maxstats);
		decode_wifstats(&range_info.avg_qual, &avgstats);

		// ALWAYS prefer quality if it's available
		if (wifstats.flags & maxstats.flags & WIF_STAT_QUAL_VALID
				&& maxstats.quality > 0
				)
			{
			hint_status = 1;
			val_max = maxstats.quality;
			val_min = 0;
			if (avgstats.flags & WIF_STAT_QUAL_VALID
					&& avgstats.quality >= val_min
					&& avgstats.quality < val_max
					)
				{
				val_good = avgstats.quality;
				val_fair = IMID(val_good, val_min, range_ratio);
				}
			else
				{
				val_good = IMID(val_max, val_min, range_ratio);
				val_fair = IMID(val_good, val_min, range_ratio);
				}
			}
		else if (wifstats.flags & maxstats.flags & WIF_STAT_SIGNAL_VALID
				&& !((qual_info->updated^range_info.max_qual.updated) & IW_QUAL_DBM)
				)
			{
			if (!(wifstats.flags & WIF_STAT_UNITS_DBM))
				{
				// relative
				hint_status = 1;
				val_max = maxstats.signal;
				val_min = 0;
				if (avgstats.flags & WIF_STAT_SIGNAL_VALID
						&& !(avgstats.flags & WIF_STAT_UNITS_DBM)
						&& avgstats.signal >= val_min
						&& avgstats.signal < val_max
						)
					{
					val_good = avgstats.signal;
					val_fair = IMID(val_good, val_min, range_ratio);
					}
				else
					{
					val_good = IMID(val_max, val_min, range_ratio);
					val_fair = IMID(val_good, val_min, range_ratio);
					}
				}
			else
				{
				// dBm of some kind
				val_min = maxstats.signal;
				if (avgstats.flags & WIF_STAT_SIGNAL_VALID
						&& !((qual_info->updated^range_info.avg_qual.updated) & IW_QUAL_DBM)
						)
					{
					hint_status = 1;
					val_good = avgstats.signal;
					val_fair = IMID(val_good, val_min, range_ratio);
					val_max = val_good + val_good - val_min;
					}
				else
					{
					hint_status = 0;
					val_max = val_min + range_granularity;
					val_good = IMID(val_max, val_min, range_ratio);
					val_fair = IMID(val_good, val_min, range_ratio);
					}
				}
			}
		}

	if (hint_status < 0 && wifstats.flags & WIF_STAT_VALID_MASK)
		{
		hint_status = 0;
		if (wifstats.flags & WIF_STAT_QUAL_VALID)
			{
			val_max = range_granularity;
			val_min = 0;
			val_good = IMID(val_max, val_min, range_ratio);
			val_fair = IMID(val_good, val_min, range_ratio);
			}
		else if (wifstats.flags & WIF_STAT_SIGNAL_VALID)
			{
			if (qual_info->updated & IW_QUAL_DBM)
				{
				val_max = range_granularity;
				val_min = 0;
				val_good = IMID(val_max, val_min, range_ratio);
				val_fair = IMID(val_good, val_min, range_ratio);
				}
			else if (qual_info->updated & IW_QUAL_RCPI)
				{
				val_min = WIF_BOGUS_DBM + 1;
				val_max = val_min + range_granularity;
				val_good = IMID(val_max, val_min, range_ratio);
				val_fair = IMID(val_good, val_min, range_ratio);
				}
			else
				{
				val_min = -110;
				val_max = val_min + range_granularity;
				val_good = IMID(val_max, val_min, range_ratio);
				val_fair = IMID(val_good, val_min, range_ratio);
				}
			}
		}
	}


void WirelessMeter::update_stats(void)
	{
	int current;
	int used;
	struct wif_stats	wifstats;
	struct iw_statistics	stat_info;

	memset(&stat_info, 0, sizeof(stat_info));
	iwrq.u.data.pointer = (caddr_t)&stat_info;
	iwrq.u.data.length  = sizeof(stat_info);
	iwrq.u.data.flags   = 0;

	if (ioctl(fd, SIOCGIWSTATS, &iwrq) < 0)
		{
		//std::cerr << iwrq.ifr_name << " = " << errno << std::endl;
		// device unavailable; reset settings.
		hint_status = -1;
		}
	else
		{
		decode_wifstats(&stat_info.qual, &wifstats);

		if (hint_status < 0)
			{
			check_hints(&stat_info.qual);
			}
		}

	if (hint_status < 0)
		{
		// can only get here if there are no stats
		val_min = 0;
		val_max = 0;
		current = val_max;
		fields_[0] = fields_[1] = fields_[2] = 0.0;
		used = 0;
		}
	else
		{
		decode_wifstats(&stat_info.qual, &wifstats);

		if (wifstats.flags & WIF_STAT_QUAL_VALID)
			{
			current = wifstats.quality;
			}
		else if (wifstats.flags & WIF_STAT_SIGNAL_VALID)
			{
			current = wifstats.signal;
			}
		else
			{
			current = 0;
			}

		if (!hint_status && current > val_max)
			{
			val_max = val_min + range_granularity
					* (int)((current-val_min+range_granularity-1)/range_granularity);
			val_good = IMID(val_max, val_min, range_ratio);
			val_fair = IMID(val_good, val_min, range_ratio);
			}

		fields_[0] = fields_[1] = fields_[2] = 0.0;
		fields_[(current >= val_good) ? 2 : (current >= val_fair) ? 1 : 0]
				= (double)(current-val_min)/(val_max-val_min);
		used = current-val_min;
		}

	fields_[3] = (double)(val_max-current)/(val_max-val_min);
	setUsed(used, wMAX(1, val_max-val_min));
	}


static int wifname_cmp(const void *p1, const void *p2)
	{
	return strcmp(((struct wif_info *)p1)->ifname,
			((struct wif_info *)p2)->ifname);
	}


int WirelessMeter::countdevices(void)
	{
	DIR		*dir;
	FILE		*f;
	struct dirent	*dent;
	struct iwreq	iwrq;

	if (wif_count >= 0)
		{
		return wif_count;
		}

	wif_count = 0;

	if (fd < 0 && (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
		std::cerr << "Can not open socket" << std::endl;
		exit(1);
		}

	if ((dir = opendir(SYS_NET)))
		{
		// sysfs seems to the direction the kernel community is moving
		// so try it first.
		memset(&iwrq, 0, sizeof(iwrq));

		while ((dent =readdir(dir)))
			{
			if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
				{
				continue;
				}

			// Use SIOCGIWNAME as indicator: if interface does not
			// support this ioctl, it has no wireless extensions.
			strncpy(iwrq.ifr_name, dent->d_name, IFNAMSIZ);
			if (ioctl(fd, SIOCGIWNAME, &iwrq) < 0)
				{
				continue;
				}

			if (!(wif_array = (struct wif_info *)realloc(
					wif_array,
					(wif_count+1)*sizeof(struct wif_info)
					)))
				{
				std::cerr << "Realloc failed" << std::endl;
				exit(1);
				}

			wif_array[wif_count].ifname = strdup(iwrq.ifr_name);

			wif_count++;
			}

		closedir(dir);
		}
	else if ((f = fopen(PROC_NET_DEV, "r")))
		{
		// Fall back to proc if sysfs fails
		char *p;
		char buffer[BUFSIZ];

		while (fgets(buffer, sizeof(buffer), f))
			{
			if ((p = strchr(buffer, ':')))
				{
				*p = '\0';
				for (p = buffer; isspace(*p);)
					{
					p++;
					}

				// Use SIOCGIWNAME as indicator: if interface does not
				// support this ioctl, it has no wireless extensions.
				strncpy(iwrq.ifr_name, p, IFNAMSIZ);
				if (ioctl(fd, SIOCGIWNAME, &iwrq) < 0)
					{
					continue;
					}

				if (!(wif_array = (struct wif_info *)realloc(
						wif_array,
						(wif_count+1)*sizeof(struct wif_info)
						)))
					{
					std::cerr << "Realloc failed" << std::endl;
					exit(1);
					}

				wif_array[wif_count].ifname = strdup(iwrq.ifr_name);

				wif_count++;
				}
			}

		fclose(f);
		}

	// This sorts the wirless devices by name. Another possibility is
	// by the kernel's net device index value. It's really just an
	// attempt to make it easier to determine the mapping of meters
	// to devices (if there is more than wireless device).
	qsort(wif_array, wif_count, sizeof(struct wif_info), wifname_cmp);

	return wif_count;
	}


const char *WirelessMeter::wirelessStr(int num)
	{
	static char buffer[8] = "WLAN";

	if (num != 1)
		{
		snprintf(buffer + 4, 3, "%d", num);
		}
	buffer[7] = '\0';

	return buffer;
	}
