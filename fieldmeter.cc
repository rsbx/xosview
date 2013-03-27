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


void FieldMeter::SetUsedFormat ( const char * const fmt ) {
    /*  Do case-insensitive compares.  */
  if (!strncasecmp (fmt, "percent", 8))
    print_ = PERCENT;
  else if (!strncasecmp (fmt, "autoscale", 10))
    print_ = AUTOSCALE;
  else if (!strncasecmp (fmt, "float", 6))
    print_ = FLOAT;
  else
  {
    fprintf (stderr, "Error:  could not parse format of '%s'\n", fmt);
    fprintf (stderr, "  I expected one of 'percent', 'bytes', or 'float'\n");
    fprintf (stderr, "  (Case-insensitive)\n");
    exit(1);
  }
}


void FieldMeter::setUsed (double val, double total)
{
  if (print_ == FLOAT)
    used_ = val;
  else if (print_ == PERCENT)
  {
    if (total != 0.0)
      used_ = val / total * 100.0;
    else
    {
      if (!printedZeroTotalMesg_) {
        printedZeroTotalMesg_ = 1;
	fprintf(stderr, "Warning:  %s meter had a zero total "
		"field!  Would have caused a div-by-zero "
		"exception.\n", name());
      }
      used_ = 0.0;
    }
  }
  else if (print_ == AUTOSCALE)
    used_ = val;
  else {
    fprintf (stderr, "Error in %s:  I can't handle a "
		     "UsedType enum value of %d!\n", name(), print_);
    exit(1);
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
  int xoffset;

  if (!dolegends_ || !dolegends_ || (!manditory && lastused_ == used_))
      return;

  parent_->setStippleN(0);	/*  Use all-bits stipple.  */

  char buf[10];

  if (print_ == PERCENT){
    snprintf( buf, 10, "%d%%", (int)used_ );
  }
  else if (print_ == AUTOSCALE){
    char scale;
    double scaled_used;
      /*  Unfortunately, we have to do our comparisons by 1000s (otherwise
       *  a value of 1020, which is smaller than 1K, could end up
       *  being printed as 1020, which is wider than what can fit)  */
      /*  However, we do divide by 1024, so a K really is a K, and not
       *  1000.  */
      /*  In addition, we need to compare against 999.5*1000, because
       *  999.5, if not rounded up to 1.0 K, will be rounded by the
       *  %.0f to be 1000, which is too wide.  So anything at or above
       *  999.5 needs to be bumped up.  */
    if (used_ >= 999.5*1000*1000*1000*1000*1000*1000)
	{scale='E'; scaled_used = used_/1024/1024/1024/1024/1024/1024;}
    else if (used_ >= 999.5*1000*1000*1000*1000)
	{scale='P'; scaled_used = used_/1024/1024/1024/1024/1024;}
    else if (used_ >= 999.5*1000*1000*1000)
	{scale='T'; scaled_used = used_/1024/1024/1024/1024;}
    else if (used_ >= 999.5*1000*1000)
	{scale='G'; scaled_used = used_/1024/1024/1024;}
    else if (used_ >= 999.5*1000)
	{scale='M'; scaled_used = used_/1024/1024;}
    else if (used_ >= 999.5)
	{scale='K'; scaled_used = used_/1024;}
    else {scale='\0'; scaled_used = used_;}
      /*  For now, we can only print 3 characters, plus the optional
       *  suffix, without overprinting the legends.  Thus, we can
       *  print 965, or we can print 34, but we can't print 34.7 (the
       *  decimal point takes up one character).  bgrayson   */
      /*  Also check for negative values, and just print "-" for
       *  them.  */
    if (scaled_used < 0)
      snprintf (buf, 10, "-");
    else if (scaled_used == 0.0)
      snprintf (buf, 10, "0");
    else if (scaled_used < 9.95) {
      //  9.95 or above would get
      //  rounded to 10.0, which is too wide.
      if (scale)
	snprintf (buf, 10, "%.1f%c", scaled_used, scale);
      else
	snprintf (buf, 10, "%.1f", scaled_used);
    } else {
      if (scale)
        snprintf (buf, 10, "%.0f%c", scaled_used, scale);
      else
        snprintf (buf, 10, "%.0f", scaled_used);
    }
  }
  else {
    snprintf( buf, 10, "%.1f", used_ );
  }

  xoffset = lastusedwidth;
  if (xoffset) {
    parent_->clear( x_ - xoffset, y_ + height_ - parent_->textHeight(),
		 xoffset, parent_->textHeight() );
  }

  xoffset = parent_->textWidth(buf);
  if (xoffset) {
    parent_->setForeground( usedcolor_ );
    parent_->drawString( x_ - xoffset, y_ + height_ - 1, buf );
  }
  lastusedwidth = xoffset;

  lastused_ = used_;
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
