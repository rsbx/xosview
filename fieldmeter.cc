//
//  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
//
//  This file may be distributed under terms of the GPL
//


#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include "fieldmeter.h"
#include "xosview.h"
#include <math.h>
#include "formatnum.h"


#define LEGEND_FIELD_SEP	"/"


FieldMeter::FieldMeter( XOSView *parent, unsigned int numfields, const char *title,
                        const char *legend, int docaptions, int dolegends,
                        int dousedlegends )
: Meter(parent, title, legend, docaptions, dolegends, dousedlegends){
    /*  We need to set print_ to something valid -- the meters
     *  apparently get drawn before the meters have a chance to call
     *  CheckResources() themselves.  */
  numWarnings_ = printedZeroTotalMesg_ = 0;
  print_ = PERCENT;
  used_ = 0;
  lastused_ = -1;
  lasttotal = 0.0;
  lastusedwidth = 0;
  fields_ = NULL;
  colors_ = NULL;
  last_start = NULL;
  last_end = NULL;
  setNumFields(numfields);
}


void FieldMeter::disableMeter ( )
{
  setNumFields(1);
  setfieldcolor (0, "gray");
  Meter::legend ("Disabled");
  // And specify the total of 1.0, so the meter is grayed out.
  total_ = 1.0;
  fields_[0] = 1.0;
}


FieldMeter::~FieldMeter( void ){
  delete[] fields_;
  delete[] colors_;
  delete[] last_start;
  delete[] last_end;
}


void FieldMeter::checkResources( void ){
  Meter::checkResources();
  usedcolor_ = parent_->allocColor( parent_->getResource( "usedLabelColor") );
}


void FieldMeter::SetUsedFormat (const char * const fmt) {
    /*  Do case-insensitive compares.  */
  if (!strcasecmp(fmt, "percent")) {
    print_ = PERCENT;
  }
  else if (!strcasecmp(fmt, "scale1000")) {
    print_ = DECIMAL;
  }
  else if (!strcasecmp(fmt, "scale1024")) {
    print_ = COMPUTER;
  }
  else if (!strcasecmp(fmt, "autoscale")) {
    print_ = COMPUTER;
    fprintf (stderr, "Warning: The 'autoscale' format specifier is deprecated.\n");
    fprintf (stderr, "  Use 'scale1024' instead.\n");
  }
  else if (!strcasecmp(fmt, "float")) {
    print_ = DECIMAL;
    fprintf (stderr, "Warning: The 'float' format specifier is deprecated.\n");
    fprintf (stderr, "  Use 'scale1000' instead.\n");
  }
  else
  {
    fprintf (stderr, "Error:  could not parse format of '%s'\n", fmt);
    fprintf (stderr, "  One of 'percent', 'scale1000', or 'scale1024' was expected\n");
    fprintf (stderr, "  (Case-insensitive)\n");
    exit(1);
  }
}


void FieldMeter::setUsed (double val, double total)
{
  used_ = val;
  total_ = total;

  if (print_ == PERCENT)
  {
    if (total != 0.0)
      used_ = val / total * 100.0;
  }
}


void FieldMeter::setfieldcolor(unsigned int field, const char *color) {
  setfieldcolor(field, parent_->allocColor(color));
}


void FieldMeter::setfieldcolor(unsigned int field, unsigned long color) {
  if (field < numfields_)
    colors_[field] = color;

  if (!baddatacolorset)
    setbaddatafieldcolor(parent_->getResource("BadDataColor"));
}


void FieldMeter::setbaddatafieldcolor(const char *color) {
  setbaddatafieldcolor(parent_->allocColor(color));
}


void FieldMeter::setbaddatafieldcolor(unsigned long color) {
  colors_[numfields_] = color;
  baddatacolorset = true;
}


void FieldMeter::drawMeterDisplay( void ){
    /*  Draw the outline for the fieldmeter.  */
  parent_->setForeground( parent_->foreground() );
  parent_->drawFilledRectangle( x_, y_, width_, height_ );
  if ( dolegends_ ){
    parent_->setForeground( textcolor_ );

    int offset;
    if ( dousedlegends_ )
      offset = parent_->textWidth( "XXXXXXXXX" );
    else
      offset = parent_->textWidth( "XXXXX" );

    parent_->drawString( x_ - offset, y_ + height_ - 1, title_ );
  }

  drawlegend();
  drawfields(1);
}


void FieldMeter::drawlegend(void) {
  int x = 0;
  unsigned int fieldcount = 0;
  char *p0;
  char *p1;
  char *p2;

  if (!docaptions_ || !dolegends_)
    return;

  parent_->clear(x_, y_-parent_->textHeight(), width_, parent_->textHeight());

  if (!(p0 = p1 = strdup(legend_))) {
    std::cerr << "strdup failed" << std::endl;
    exit(1);
  }
  p1 = strtok_r(p1, LEGEND_FIELD_SEP, &p2);
  while (p1) {
    fieldcount++;
    p1 = strtok_r(NULL, LEGEND_FIELD_SEP, &p2);
  }
  free(p0);

  if (fieldcount == numfields_) {
    int i = 0;

    if (!(p0 = p1 = strdup(legend_))) {
      std::cerr << "strdup failed" << std::endl;
      exit(1);
    }
    if ((p1 = strtok_r(p1, LEGEND_FIELD_SEP, &p2))) {
      parent_->setStippleN(i%4);
      parent_->setForeground(colors_[i]);
      parent_->drawString(x_+x, y_-1, p1);
      x += parent_->textWidth(p1);
    }
    while (p1) {
      i++;
      if ((p1 = strtok_r(NULL, LEGEND_FIELD_SEP, &p2))) {
        parent_->setForeground(parent_->foreground());
        parent_->setStippleN(0);
        parent_->drawString(x_+x, y_-1, "/");
        x += parent_->textWidth("/", 1);

        parent_->setStippleN(i%4);
        parent_->setForeground(colors_[i]);
        parent_->drawString(x_+x, y_-1, p1);
        x += parent_->textWidth(p1);
      }
    }
    free(p0);
    parent_->setStippleN(0);
  } else {
    parent_->setForeground(textcolor_);
    parent_->drawString(x_+x, y_-1, legend_);
  }
}


void FieldMeter::drawused(int manditory) {
  char buf[10];

  if (!dolegends_
        || !dousedlegends_
        || (!manditory && lastused_ == used_ && lasttotal == total_)
        )
    return;

  lastused_ = used_;
  lasttotal = total_;

  parent_->setStippleN(0);	/*  Use all-bits stipple.  */

  if (lastusedwidth) {
    parent_->clear(x_-lastusedwidth, y_+height_-parent_->textHeight(),
                 lastusedwidth, parent_->textHeight());
    lastusedwidth = 0;
  }

  if (total_ == 0.0)
    return;

  if (print_ == PERCENT) {
    (void)FormatNum4(buf, 10, FormatNum_Percent, used_);
  }
  else if (print_ == COMPUTER) {
    (void)FormatNum4(buf, 10, FormatNum_Scale_1024_Up, used_);
  }
  else if (print_ == DECIMAL) {
    (void)FormatNum4(buf, 10, FormatNum_Scale_1000_Up, used_);
  }

  lastusedwidth = parent_->textWidth(buf);
  if (lastusedwidth) {
    parent_->setForeground(usedcolor_);
    parent_->drawString(x_-lastusedwidth, y_+height_-1, buf);
  }
}


void FieldMeter::updateMeterDisplay(void) {
  drawfields(0);
}


void FieldMeter::updateMeterHistory(void) {
}


void FieldMeter::drawfields(int manditory) {
  unsigned int i;
  int start, end;
  double runningtotal = 0.0;
  double total = 0.0;

  if (dousedlegends_)
    drawused( manditory );

  if (!(width_ > 2*BORDER_WIDTH && height_ > 2*BORDER_WIDTH)) {
    return;
  }

  for (i=0; i<numfields_; i++) {
    if (fields_[i] > 0.0)
      total += fields_[i];
    }

  if (!(total > 0.0)) {
    fields_[numfields_] = 1.0;
    total = 1.0;
  }
  else {
    fields_[numfields_] = 0.0;
  }

  start = 0;
  for (i=0; i<=numfields_; i++) {
    if (fields_[i] > 0.0)
      runningtotal += fields_[i];
    end = floor((width_-2*BORDER_WIDTH)*(runningtotal/total) - 0.5);
    if (end >= start && (manditory || (start != last_start[i]) || (end != last_end[i]))) {
      parent_->setForeground(colors_[i]);
      parent_->setStippleN(i%4);
      parent_->drawFilledRectangle(x_+start+BORDER_WIDTH, y_+BORDER_WIDTH, end-start+1, height_-2*BORDER_WIDTH);
      parent_->setStippleN(0);  /*  Restore all-bits stipple.  */
      last_start[i] = start;
      last_end[i] = end;
    }
    start = end+1;
  }
}


void FieldMeter::setNumFields(unsigned int n) {
  numfields_ = n;
  delete[] fields_;
  delete[] colors_;
  delete[] last_start;
  delete[] last_end;
  fields_ = new double[numfields_+1];
  colors_ = new unsigned long[numfields_+1];
  last_start = new int[numfields_+1];
  last_end = new int[numfields_+1];
  baddatacolorset = false;

  total_ = 0;
  for (unsigned int i=0; i<=numfields_; i++) {
    fields_[i] = 0.0;
    last_start[i] = last_end[i] = -1;
  }
  fields_[numfields_] = 1.0;
}
