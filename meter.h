//
//  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
//
//  This file may be distributed under terms of the GPL
//

#ifndef _METER_H_
#define _METER_H_

#include <stdio.h>
#include "xosview.h"	//  To grab MAX_SAMPLES_PER_SECOND.

class XOSView;

class Meter {
public:
  Meter( XOSView *parent, const char *title = "", const char *legend ="",
	 int docaptions = 0, int dolegends = 0, int dousedlegends = 0 );
  virtual ~Meter( void );

  virtual const char *name(void) const { return "Meter"; }
  virtual void resize(int x, int y, int width, int height);
  virtual void checkevent(void) = 0;
  virtual void drawMeterDisplay(void) = 0;
  virtual void updateMeterDisplay(void) = 0;
  void title( const char *title );
  const char *title( void ) { return title_; }
  void legend( const char *legend );
  const char *legend( void ) { return legend_; }
  void docaptions( int val ) { docaptions_ = val; }
  void dolegends( int val ) { dolegends_ = val; }
  void dousedlegends( int val ) { dousedlegends_ = val; }
  bool requestevent( unsigned long counter ){
    if (priority_ == 0) {
      fprintf(stderr, "Warning:  meter %s had an invalid priority "
	      "of 0.  Resetting to 1...\n", name());
      priority_ = 1;
    }
    return (counter % priority_) == 0;
  }

  int getX() const { return x_; }
  int getY() const { return y_; }
  int getWidth() const { return width_; }
  int getHeight() const { return height_; }

  virtual void checkResources(void) = 0;
  virtual void updateMeterHistory(void) = 0;

protected:
  XOSView *parent_;
  int x_, y_, width_, height_, docaptions_, dolegends_, dousedlegends_;
  unsigned int priority_;
  char *title_, *legend_;
  unsigned long textcolor_;
  double samplesPerSecond() { return 1.0*MAX_SAMPLES_PER_SECOND/priority_; }
  double secondsPerSample() { return 1.0/samplesPerSecond(); }

private:
};

#endif
