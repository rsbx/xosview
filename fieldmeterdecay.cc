//
//  The original FieldMeter class is Copyright (c) 1994, 2006 by Mike Romberg
//    ( mike.romberg@noaa.gov )
//  Modifications from FieldMeter class done in Oct. 1995
//    by Brian Grayson ( bgrayson@netbsd.org )
//
//  This file was written by Brian Grayson for the NetBSD and xosview
//    projects.
//  This file may be distributed under terms of the GPL or of the BSD
//    license, whichever you choose.  The full license notices are
//    contained in the files COPYING.GPL and COPYING.BSD, which you
//    should have received.  If not, contact one of the xosview
//    authors for a copy.
//

// In order to use the FieldMeterDecay class in place of a FieldMeter class in
// a meter file (say, cpumeter.cc), make the following changes:
//   1.  Change cpumeter.h to include fieldmeterdecay.h instead of
//       fieldmeter.h
//   2.  Change CPUMeter to inherit from FieldMeterDecay, rather than
//       FieldMeter.
//   3.  Change the constructor call to use FieldMeterDecay(), rather than
//       FieldMeter().
//   4.  Make the checkResources () function in the meter set the
//	 dodecay_ variable according to the, e.g., xosview*cpuDecay resource.

#include <iostream>
#include <fstream>
#include <math.h>		//  For fabs()
#include "fieldmeter.h"
#include "fieldmeterdecay.h"
#include "xosview.h"
#include "math.h"


//  The constant below can be modified for quicker or slower
//  exponential rates for the average.  No fancy math is done to
//  set it to correspond to a five-second decay or anything -- I
//  just played with it until I thought it looked good!  :)  BCG
#define ALPHA 0.97


FieldMeterDecay::FieldMeterDecay( XOSView *parent,
                int numfields, const char *title,
                const char *legend, int docaptions, int dolegends,
                int dousedlegends )
	: FieldMeter (parent, numfields, title, legend, docaptions, dolegends,
              dousedlegends)
{
  firsttime_ = 1;
  dodecay_ = 1;
  decay_ = NULL;
  last_f_start = NULL;
  last_f_end = NULL;
  last_d_start = NULL;
  last_d_end = NULL;

  setNumDecayFields();
}

FieldMeterDecay::~FieldMeterDecay(void) {
  delete[] decay_;
  delete[] last_f_start;
  delete[] last_f_end;
  delete[] last_d_start;
  delete[] last_d_end;
}

void FieldMeterDecay::setNumDecayFields(void) {
  int i;

  delete[] decay_;
  delete[] last_f_start;
  delete[] last_f_end;
  delete[] last_d_start;
  delete[] last_d_end;

  decay_ = new double[numfields_];
  last_f_start = new int[numfields_];
  last_f_end = new int[numfields_];
  last_d_start = new int[numfields_];
  last_d_end = new int[numfields_];

  for (i=0; i<numfields_; i++) {
    decay_[i] = 0.0;
    last_f_start[i] = -1;
    last_f_end[i] = -1;
    last_d_start[i] = -1;
    last_d_end[i] = -1;
  }

  firsttime_ = 1;
}

void FieldMeterDecay::setNumFields(int n) {
  FieldMeter::setNumFields(n);
  FieldMeterDecay::setNumDecayFields();
}

void FieldMeterDecay::drawfields(int manditory)
  {
  int i;
  int start, end;
  int halfheight;
  double runningtotal;
  double total_f, total_d;

  if (!dodecay_)
    {
    //  If this meter shouldn't be done as a decaying splitmeter,
    //  call the ordinary fieldmeter code.
    FieldMeter::drawfields(manditory);
    return;
    }

  if (dousedlegends_)
    drawused(manditory);

  total_f = 0.0;
  for (i=0; i<numfields_; i++)
    {
    if (fields_[i] > 0.0)
      total_f += fields_[i];
    }

  if (firsttime_ && total_f > 0.0)
    {
    firsttime_ = 0;
    manditory = 1;
    for (int i = 0; i < numfields_; i++)
      {
      decay_[i] = (fields_[i] > 0.0) ? fields_[i]/total_f : 0.0;
      }
    }

  total_d = 0.0;
  for (i=0; i< numfields_; i++)
    {
    decay_[i] *= ALPHA;
    if (total_f > 0.0 && fields_[i] > 0.0)	// If this is false, decay_[] becomes denormalized but will recover
      decay_[i] += (1-ALPHA)*(fields_[i]/total_f);
    total_d += decay_[i];
    }

  halfheight = height_/2-BORDER_WIDTH;
  halfheight = (halfheight > 0) ? halfheight : 0;

  if (width_-2*BORDER_WIDTH > 0 && height_-2*BORDER_WIDTH-halfheight > 0 && total_f > 0.0)
    {
    start = 0;
    runningtotal = 0.0;
    for (i=0; i< numfields_; i++)
      {
      if (fields_[i] > 0.0)
        runningtotal += fields_[i];

      if (total_f > 0.0)
        {
        end = floor((width_-2*BORDER_WIDTH)*(runningtotal/total_f) - 0.5);
        if (end >= start && (manditory || (start != last_f_start[i]) || (end != last_f_end[i])))
          {
          parent_->setForeground(colors_[i]);
          parent_->setStippleN(i%4);
          parent_->drawFilledRectangle(x_+start+BORDER_WIDTH, y_+BORDER_WIDTH, end-start+1, height_-2*BORDER_WIDTH-halfheight);
          parent_->setStippleN(0);
          }
        last_f_start[i] = start;
        last_f_end[i] = end;
        start = end+1;
        }
      }
    }

  total_d = 1.0;	// The decay_[] array is normalized so that the sum is always 1.0
  if (width_-2 > 0 && halfheight > 0 && total_d > 0.0)
    {
    start = 0;
    runningtotal = 0.0;
    for (i=0; i< numfields_; i++)
      {
      if (decay_[i] > 0.0)
        runningtotal += decay_[i];

      if (total_d > 0.0)
        {
        end = floor((width_-2*BORDER_WIDTH)*(runningtotal/total_d) - 0.5);
        if (end >= start && (manditory || (start != last_d_start[i]) || (end != last_d_end[i])))
          {
          parent_->setForeground(colors_[i]);
          parent_->setStippleN(i%4);
          parent_->drawFilledRectangle(x_+start+BORDER_WIDTH, y_+BORDER_WIDTH+height_-2*BORDER_WIDTH-halfheight, end-start+1, halfheight);
          parent_->setStippleN(0);
          }
        last_d_start[i] = start;
        last_d_end[i] = end;
        start = end+1;
        }
      }
    }
  }
