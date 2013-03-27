//
//  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
//
//  This file may be distributed under terms of the GPL
//

#include <math.h>

#include "bitmeter.h"
#include "xosview.h"

BitMeter::BitMeter( XOSView *parent,
		    const char *title, const char *legend, int numBits,
		    int docaptions, int, int dousedlegends)
  : Meter( parent, title, legend, docaptions, dousedlegends, dousedlegends ),
  bits_(NULL), lastbits_(NULL), disabled_(false)  {
  setNumBits(numBits);
}

BitMeter::~BitMeter( void ){
  delete [] bits_;
  delete [] lastbits_;
}

void BitMeter::setNumBits(int n){
  numbits_ = n;
  delete [] bits_;
  delete [] lastbits_;

  bits_ = new char[numbits_];
  lastbits_ = new char[numbits_];

  for ( int i = 0 ; i < numbits_ ; i++ )
      bits_[i] = lastbits_[i] = 0;
}

void BitMeter::disableMeter ( void ) {
  disabled_ = true;
  onColor_ = parent_->allocColor ("gray");
  offColor_ = onColor_;
  Meter::legend ("Disabled");

}

void BitMeter::checkResources( void ){
  Meter::checkResources();
}

void BitMeter::updateMeterDisplay(void) {
  drawBits(0);
}


void BitMeter::updateMeterHistory(void) {
}


void BitMeter::drawBits(int manditory) {
  int x1 = 0, x2;

  // Draw all or none
  if (!(width_-2*BORDER_WIDTH >= 2*numbits_-1 && height_-2*BORDER_WIDTH > 0))
    return;

  x1 = 0;
  for (int i=0; i<numbits_; i++) {
    x2 = floor((width_-2*BORDER_WIDTH+1)*((double)(i+1)/numbits_) - 0.5)-1;

    if (x2 >= x1 && (manditory || (bits_[i] != lastbits_[i]))) {
      parent_->setForeground( bits_[i] ? onColor_ : offColor_ );
      parent_->drawFilledRectangle(x_+BORDER_WIDTH+x1, y_+BORDER_WIDTH, x2-x1+1, height_-2*BORDER_WIDTH);
    }

    lastbits_[i] = bits_[i];
    x1 = x2 + 2;
  }
}

void BitMeter::drawMeterDisplay(void) {
  parent_->setForeground( parent_->foreground() );
  parent_->drawFilledRectangle( x_, y_, width_, height_ );
  if ( dolegends_ ) {
    parent_->setForeground( textcolor_ );

    int offset;
    if ( dousedlegends_ )
      offset = parent_->textWidth( "XXXXXXXXX" );
    else
      offset = parent_->textWidth( "XXXXX" );

    parent_->drawString( x_ - offset, y_ + height_ - 1, title_ );
    parent_->setForeground( onColor_ );
    if(docaptions_)
    {
      parent_->drawString( x_, y_ - 1, legend_ );
    }
  }

  drawBits( 1 );
}

void BitMeter::setBits(int startbit, unsigned char values){
  unsigned char mask = 1;
  for (int i = startbit ; i < startbit + 8 ; i++){
    bits_[i] = values & mask;
    mask = mask << 1;
  }
}
