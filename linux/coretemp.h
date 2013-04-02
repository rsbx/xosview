//
//  Copyright (c) 2008 by Tomi Tapper <tomi.o.tapper@jyu.fi>
//
//  File based on linux/lmstemp.* by
//  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
//
//  This file may be distributed under terms of the GPL
//
//
//
#ifndef _CORETEMP_H_
#define _CORETEMP_H_

#include "cpumeter.h"
#include "fieldmeter.h"
#include "MeterMaker.h"
#include <string>
#include <vector>


class CoreTemp : public FieldMeter {
public:
  CoreTemp( XOSView *parent, const char *label, const char *caption, int pkg, int cpu);
  ~CoreTemp( void );

  static void makeMeters(XOSView *xosview, MeterMaker *mmake);
  const char *name( void ) const { return "CoreTemp"; }
  void checkevent( void );
  void checkResources( void );
  static unsigned int countCpus(int pkg);

protected:
  void getcoretemp( void );

private:
  int _pkg, _cpu, _high;
  std::vector<std::string> _cpus;
  unsigned long _actcolor, _highcolor;
};


#endif
