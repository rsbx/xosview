//
//  Original FieldMeter class is Copyright (c) 1994, 2006 by Mike Romberg
//    ( mike.romberg@noaa.gov )
//
//  Modifications from FieldMeter class done in Oct. 1995
//    by Brian Grayson ( bgrayson@netbsd.org )
//
//  Modifications from FieldMeterDecay class done in Oct. 1998
//    by Scott McNab ( jedi@tartarus.uwa.edu.au )
//

#ifndef _FIELDMETERGRAPH_H_
#define _FIELDMETERGRAPH_H_

#include "meter.h"
#include "fieldmeterdecay.h"

class FieldMeterGraph : public FieldMeterDecay {
public:
  FieldMeterGraph( XOSView *parent, unsigned int numfields,
              const char *title = "", const char *legend = "",
              int docaptions = 0, int dolegends = 0, int dousedlegends = 0 );
  virtual ~FieldMeterGraph( void );

protected:
  virtual void setNumCols(unsigned int n);
  virtual void setNumFields(unsigned int n);
  virtual void drawfields(int manditory = 0);

  int useGraph_;
  unsigned int graphNumCols_;
  unsigned int graphpos_;
  /*  There's some sort of corruption going on -- we can't have
   *  variables after the heightfield_ below, otherwise they get
   *  corrupted???  */
  double *heightfield_;

private:
  int last_x, last_y, last_width, last_height;
  void checkResize(void);
  void drawBar(unsigned int i);
  enum XOSView::windowVisibilityState lastWinState;

};

#endif
