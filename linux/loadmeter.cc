//
//  Copyright (c) 1994, 1995, 2006, 2008 by Mike Romberg ( mike.romberg@noaa.gov )
//  Copyright (c) 2009, 2010, 2013 by Raymond S Brand <rsbx@acm.org>
//
//  This file may be distributed under terms of the GPL
//  The changes by Raymond S Brand may be distributed under the terms of (any)
//  BSD license or equivalent.

//  Most of this code was written by Werner Fink <werner@suse.de>.
//  Only small changes were made on my part (M.R.)
//  Almost entirely rewitten by Raymond S Brand.

#include "loadmeter.h"
#include "xosview.h"
#include "cpumeter.h"
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//  Prior to the rewrite the displayed scale range was unpredictable making it
//  impossible to estimate the system load average when usedLabels was not
//  enabled.
//    The scale range was determined as follows:
//      + The scale range is always >= 1.0
//      + If the current load value is > the current scale range,
//          the scale range becomes 5 times the current load value.
//      + If the current load value < 1/5 the current scale range,
//          the scale range becomes the greater of 1.0 and the current load
//          value.
//    The bar color was determined as follows with the default resources:
//      + The color of the unused part of the display was set to "loadIdleColor".
//      + If the current load average was less than or equal to the CPU count,
//          the color was set to loadProcColor.
//      + If the current load average was less than or equal to 4 * CPU count,
//          the color was set to "loadWarnColor".
//      + Otherwise, the the color was set to "loadCritColor".
//
//  After the rewrite the displayed scale range is predictable and the bar
//  also indicates what the displayed scale range is.
//
//  Theory:
//
//  The current load avgerage value (L) is converted into a form like
//  exponential or scientific noatation. The components of this form are:
//    + Fract:  "fractional part"; real; [0, 1]
//    + B:      "base for the exponential multiplier"; real; (1, infinity)
//    + E:      "power for the exponential multiplier"; integer; [0, infinity)
//  Such that:
//    + L = Fract * B^E
//    + Fract is the largest value that conforms to the above constraints.
//  If E is greater than or equal to the number of colors listed in the
//  "loadColorList", the meter is "pegged"; E is set to the color count - 1
//  and Fract is set to 1.0.
//
//  The high end of the displayed scale range is B^E and E determines the bar
//  color.
//
//  With the default Resources, the meanings of the colors has not changed;
//  only how the scale range is determined.
//
//  Behaviors:
//
//    The following display behaviors are avaialabe and selected by the
//    "loadScalingBehavior" resource:
//      + "Traditional"
//          This behavior is similar to what the old code did.
//            + The low end of the displayed scale range is 0.0.
//            + The bar length is set to Fract.
//            + The color of the "idle" part of the display is set to
//                "loadIdleColor".
//      + "Overlay"
//          This behavior provides a display that appears to grow from the low
//              end of the scale, as the current load avgerage increases,
//              overlaying the previous display.
//            + If E is 0:
//              + The low end of the displayed scale range is 0.
//              + The bar length is Fract.
//              + The color of the "idle" part of the display is set to
//                  "loadIdleColor".
//            + IF is not 0:
//              + The low end of the displayed scale range is E/B.
//              + The bar length is Fract - 1/B.
//              + The color of the "idle" part of the display is set to
//                  the bar color for E-1.
//
//  Variables:
//
//  Inputs:
//    L         "load average"
//              float, [0, infinity)
//    C         "CPU count"
//              float, (0, infinity)
//    B         "Base"
//              float, (1, infinity)
//    Colors    "color count"
//              integer, [1, infinity), see below
//
//  Working variables:
//    A         "adjusted load average"
//              float, L/C
//    l2_B      "log2(B)"
//              float, log2(B)
//    lB_A      "logB(A)"
//              float, log2(A)/l2_B
//    E         "exponent"
//              integer, MIN(Colors-1, MAX(0, ceil(lB_A)))
//    R_MAX     "adjusted scale range MAX"
//              float, exp2(E*l2_B)
//    R_MIN     "adjusted scale range MIN"
//              float, R_MAX/B or 0, see below
//    Bar_idx   "color index for load bar"
//              integer, ColorCount-1 - E;
//    Idle_idx  "color index for ``idle'' bar"
//              integer, Bar_idx+1 or Colors, see below
//
//  Outputs:
//    Bar       "load bar length"
//              float, [0.0, 1.0], (MIN(A, R_MAX) - R_min) / (R_MAX-R_min)
//    Idle      "idle bar length"
//              float, [0.0, 1.0], 1.0 - Bar
//    Used      "load average"
//              float, [0, infinity), L
//    Total_    "total displayed"
//              float, 1.0
//

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


#define DEFAULT_LOADFILENAME "/proc/loadavg"
static const char *LOADFILENAME = NULL;		// set in checkResources()
static const char SPEEDFILENAME[] = "/proc/cpuinfo";

#define RESOURCE_SEP	" \t,;"


static const char *ColorList = NULL;
static int ColorCount = 0;

static int GetColorCount(XOSView *parent) {
  char *p0;
  char *p1;
  char *p2;

  if (ColorCount)
    return ColorCount;

  ColorList = parent->getResource("loadColorList");

  if (!(p0 = p1 = strdup(ColorList))) {
    std::cerr << "strdup failed" << std::endl;
    exit(1);
  }

  if ((p2 = strchr(p1, '!')))
    *p2 = '\0';
  p1 = strtok_r(p1, RESOURCE_SEP, &p2);
  while (p1) {
    ColorCount++;
    p1 = strtok_r(NULL, RESOURCE_SEP, &p2);
  }

  free(p0);
  return ColorCount;
}


LoadMeter::LoadMeter( XOSView *parent )
  : FieldMeterGraph( parent, GetColorCount(parent)+1, "LOAD", "PROCS", 1, 1, 0 ){
  old_cpu_speed_= 0;
  do_cpu_speed = 0;
}

LoadMeter::~LoadMeter( void ){
}

void LoadMeter::checkResources( void ){
  int i = 0;
  char *p0;
  char *p1;
  char *p2;

  FieldMeterGraph::checkResources();

  // override the default for testing
  LOADFILENAME = (parent_->getResourceOrUseDefault("loadFileName", DEFAULT_LOADFILENAME));

  priority_ = atoi (parent_->getResource( "loadPriority" ) );
  useGraph_ = parent_->isResourceTrue( "loadGraph" );
  dodecay_ = parent_->isResourceTrue( "loadDecay" );
  SetUsedFormat (parent_->getResource("loadUsedFormat"));
  do_cpu_speed  = parent_->isResourceTrue( "loadCpuSpeed" );

  if (!(p0 = p1 = strdup(ColorList))) {
    std::cerr << "strdup failed" << std::endl;
    exit(1);
  }

  if ((p2 = strchr(p1, '!')))
    *p2 = '\0';
  p1 = strtok_r(p1, RESOURCE_SEP, &p2);
  while (p1) {
    setfieldcolor(ColorCount-++i, parent_->allocColor(p1));
    p1 = strtok_r(NULL, RESOURCE_SEP, &p2);
  }
  setfieldcolor(ColorCount, parent_->getResource("loadIdleColor"));
  free(p0);

  if (!(p0 = p1 = strdup(parent_->getResource("loadScalingAdjust")))) {
    std::cerr << "strdup failed" << std::endl;
    exit(1);
  }
  if ((p2 = strchr(p1, '!')))
    *p2 = '\0';
  p1 = strtok_r(p1, RESOURCE_SEP, &p2);
  if (!strcasecmp(p1, "auto")) {
      C = CPUMeter::countCPUs();
  } else {
      C = atof(p1);
      if (C == 0.0) {
        std::cerr << "loadScalingAdjust can not be 0" << std::endl;
        exit(1);
      }
      if (C < 0.0) {
        C *= -CPUMeter::countCPUs();
      }
  }
  free(p0);

  B = atof(parent_->getResource("loadScalingBase"));
  if (B < 1.0) {
    std::cerr << "loadScalingBase must be > 1.0" << std::endl;
    exit(1);
  }
  if (!(p0 = p1 = strdup(parent_->getResource("loadScalingBehavior")))) {
    std::cerr << "strdup failed" << std::endl;
    exit(1);
  }
  if ((p2 = strchr(p1, '!')))
    *p2 = '\0';
  p1 = strtok_r(p1, RESOURCE_SEP, &p2);
  if (!strcasecmp(p1, "traditional"))
    Behavior = 0;
  else if (!strcasecmp(p1, "overlay"))
    Behavior = 1;
  else {
    Behavior = 0;
    std::cerr << "Unsupported loadScalingBehavior resource value" << std::endl;
  }
  free(p0);

  l2_B = log2(B);
  Bar_index_old = 0;
  Idle_index_old = 0;
  for (i=0; i<=ColorCount; i++)
    fields_[i] = 0.0;
}


// just check /proc/cpuinfo for the speed of cpu
// (average multi-cpus on different speeds)
// (yes - i know about devices/system/cpu/cpu*/cpufreq )
static int getspeedinfo( void ){
  std::ifstream speedinfo(SPEEDFILENAME);
  std::string line, val;
  unsigned int total_cpu = 0, ncpus = 0;

  while ( speedinfo.good() ) {
    std::getline(speedinfo, line);
    if ( strncmp(line.c_str(), "cpu MHz", 7) == 0 ) {
      val = line.substr(line.find_last_of(':') + 1);
      XOSDEBUG("SPEED: %s\n", val.c_str());
      total_cpu += atoi(val.c_str());
      ncpus++;
    }
  }

  if (ncpus > 0)
    return total_cpu / ncpus;

  return 0;
}


void LoadMeter::checkevent( void ){
  getloadinfo();
  if ( do_cpu_speed ) {
    int cpu_speed = getspeedinfo();

    if (old_cpu_speed_ != cpu_speed) {
      old_cpu_speed_ = cpu_speed;
      // update the legend:
      std::ostringstream legnd;
      XOSDEBUG("SPEED: %d\n", cpu_speed);
      legnd << "PROCS @ " << cpu_speed << " MHz"<< std::ends;
      legend( legnd.str().c_str() );
      drawlegend();
    }
  }
}


void LoadMeter::getloadinfo(void) {
  double A;
  double L;
  double lB_A;
  int E;
  double R_MAX, R_min;
  int Bar_index, Idle_index;
  std::ifstream loadinfo( LOADFILENAME );

  if ( !loadinfo ){
    std::cerr << "Can not open file : " <<LOADFILENAME << std::endl;
    parent_->done(1);
    return;
  }

  loadinfo >> L;
  loadinfo.close();

  A = L/C;
  lB_A = (A > 0.0) ? log2(A)/l2_B : 0;
  E = MIN(ceil(MAX(lB_A, 0)), ColorCount-1);
  Bar_index = ColorCount-1 - E;
  R_MAX = exp2(E*l2_B);

  if (Behavior == 1) {
    R_min = (E > 0.0) ? R_MAX/B : 0;
    Idle_index = (E > 0.0) ? Bar_index+1 : ColorCount;
  } else {
    R_min = 0;
    Idle_index = ColorCount;
  }

  fields_[Bar_index_old] = 0.0;
  fields_[Idle_index_old] = 0.0;
  total_ = 1.0;
  fields_[Bar_index] = (MIN(A, R_MAX)-R_min)/(R_MAX-R_min);
  fields_[Idle_index] = total_ - fields_[Bar_index];
  Bar_index_old = Bar_index;
  Idle_index_old = Idle_index;
  setUsed(L, MAX(C, total_));
}
