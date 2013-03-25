//
//  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
//
//  This file may be distributed under terms of the GPL
//
//  Most of this code was written by Werner Fink <werner@suse.de>
//  Only small changes were made on my part (M.R.)
//

#ifndef _LOADMETER_H_
#define _LOADMETER_H_


#include "fieldmetergraph.h"


class LoadMeter : public FieldMeterGraph {
public:
  LoadMeter( XOSView *parent );
  ~LoadMeter( void );

  const char *name( void ) const { return "LoadMeter"; }
  void checkevent( void );

  void checkResources( void );
protected:

  void getloadinfo( void );

private:
   int do_cpu_speed, old_cpu_speed_;

   double B;
   double l2_B;
   int Behavior;
   double C;
   int Bar_index_old;
   int Idle_index_old;
};


#endif
