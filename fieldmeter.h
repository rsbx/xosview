//
//  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
//
//  This file may be distributed under terms of the GPL
//

#ifndef _FIELDMETER_H_
#define _FIELDMETER_H_

#include "meter.h"
#include "timer.h"

class FieldMeter : public Meter {
public:
  FieldMeter( XOSView *parent, unsigned int numfields,
	      const char *title = "", const char *legend = "",
	      int docaptions = 0, int dolegends = 0, int dousedlegends = 0 );
  virtual ~FieldMeter( void );

  void setfieldcolor(unsigned int field, const char *color);
  void setfieldcolor(unsigned int field, unsigned long color);
  void setbaddatafieldcolor(const char *color);
  void setbaddatafieldcolor(unsigned long color);
  void docaptions( int val ) { docaptions_ = val; }
  void dolegends( int val ) { dolegends_ = val; }
  void dousedlegends( int val ) { dousedlegends_ = val; }
  void reset( void );

  void setUsed(double val, double total);
  void drawMeterDisplay(void);
  void updateMeterDisplay(void);
  void disableMeter(void);

  virtual void checkResources(void);
  virtual void updateMeterHistory(void);

protected:
  enum UsedType {INVALID_0, DECIMAL, PERCENT, COMPUTER, INVALID_TAIL};

  unsigned int numfields_;
  double *fields_;
  double total_, used_;
  int lastusedwidth;
  int *last_start, *last_end;
  unsigned long *colors_;
  unsigned long usedcolor_;
  UsedType print_;
  int printedZeroTotalMesg_;
  int numWarnings_;

  void SetUsedFormat ( const char * const str );
  void drawlegend( void );
  void drawused( int manditory );
  bool checkX(int x, int width) const;

  virtual void drawfields( int manditory = 0 );
  virtual void setNumFields(unsigned int n);

private:
  Timer _timer;
  bool baddatacolorset;
  double lastused_, lasttotal;

protected:
  void IntervalTimerStart() { _timer.start(); }
  void IntervalTimerStop() { _timer.stop(); }
  //  Before, we simply called _timer.report(), which returns usecs.
  //  However, it suffers from wrap/overflow/sign-bit problems, so
  //  instead we use doubles for everything.
  double IntervalTimeInMicrosecs() { return _timer.report_usecs(); }
  double IntervalTimeInSecs() { return _timer.report_usecs()/1e6; }
};

#endif
