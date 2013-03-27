//
//  The original FieldMeter class is Copyright (c) 1994, 2006 by Mike Romberg
//    ( mike.romberg@noaa.gov )
//
//  Modifications from FieldMeter class done in Oct. 1995
//    by Brian Grayson ( bgrayson@netbsd.org )
//
//  Modifications from FieldMeterDecay class done in Oct. 1998
//    by Scott McNab ( jedi@tartarus.uwa.edu.au )
//

// In order to use the FieldMeterGraph class in place of a FieldMeter class in
// a meter file (say, cpumeter.cc), make the following changes:
//   1.  Change cpumeter.h to include fieldmetergraph.h instead of
//       fieldmeter.h
//   2.  Change CPUMeter to inherit from FieldMeterGraph, rather than
//       FieldMeter.
//   3.  Change the constructor call to use FieldMeterGraph(), rather than
//       FieldMeter().
//   4.  Make the meter call FieldMeterGraph::checkResources(),
//       to pick up graphNumCols resource.
//   5.  Make the checkResources () function in the meter set the
//       useGraph_ variable according to the, e.g., xosview*cpuGraph resource.

#include <fstream>
#include <math.h>		//  For fabs()
#include "fieldmeter.h"
#include "fieldmetergraph.h"
#include "xosview.h"


FieldMeterGraph::FieldMeterGraph( XOSView *parent,
		unsigned int numfields, const char *title,
		const char *legend, int docaptions, int dolegends,
		int dousedlegends )
	: FieldMeterDecay (parent, numfields, title, legend, docaptions, dolegends, dousedlegends)
{
	useGraph_ = 0;
	heightfield_ = NULL;
	lastWinState = XOSView::OBSCURED;

	last_x = -1;
	last_y = -1;
	last_width = -1;
	last_height = -1;
}


FieldMeterGraph::~FieldMeterGraph( void )
{
	delete [] heightfield_;
}


void FieldMeterGraph::updateMeterHistory(void)
{
	unsigned int i,j;
	double total;

	if( !useGraph_ )
	{
		// Call FieldMeterDecay code if this meter should not be
		// drawn as a graph
		FieldMeterDecay::updateMeterHistory();
		return;
	}

	if (!sampleHistoryCount)
	{
		return;
	}

	sampleIndex = (sampleIndex+1)%sampleHistoryCount;

	// allocate memory for height field graph storage
	// note: this is done here as it is not certain that both
	// numfields_ and sampleHistoryCount are defined in the constructor
	if( heightfield_ == NULL )
	{
		if( sampleHistoryCount )
		{
			heightfield_ = new double [(numfields_+1)*sampleHistoryCount];

			for( i = 0; i < sampleHistoryCount; i++ )
			{
				for( j = 0; j < numfields_; j++ )
				{
					heightfield_[i*(numfields_+1)+j] = 0.0;
				}
				heightfield_[i*(numfields_+1)+numfields_] = 1.0;
			}
		}
	}

	if (!heightfield_)
		return;

	total = 0.0;
	for (i=0; i< numfields_; i++)
		total += (fields_[i] > 0.0) ? fields_[i] : 0.0;

	if (!(total > 0.0))
	{
		fields_[numfields_] = 1.0;
		total = 1.0;
	}
	else
	{
		fields_[numfields_] = 0.0;
	}

	// store current values for graphing
	for( i = 0; i <= numfields_; i++ )
		heightfield_[sampleIndex*(numfields_+1)+i] = (fields_[i] > 0.0) ? fields_[i]/total : 0.0;
}


void FieldMeterGraph::drawfields( int manditory )
{
	unsigned int i;
	enum XOSView::windowVisibilityState currWinState;

	if( !useGraph_ )
	{
		// Call FieldMeterDecay code if this meter should not be
		// drawn as a graph
		FieldMeterDecay::drawfields( manditory );
		return;
	}

	if ( dousedlegends_ )
		drawused( manditory );

	checkResize();

	if (!heightfield_ || !sampleHistoryCount || !(width_ > 2*BORDER_WIDTH && height_ > 2*BORDER_WIDTH))
		return;

	currWinState = parent_->getWindowVisibilityState();

	// Try to avoid having to redraw everything.
	if (!manditory && currWinState == XOSView::FULLY_VISIBLE && currWinState == lastWinState)
	{
		int sx = x_ + BORDER_WIDTH + 1;
		int swidth = width_ - 2*BORDER_WIDTH -1;
		int sheight = height_ - 2*BORDER_WIDTH;
		if( swidth > 0 && sheight > 0 )
			parent_->copyArea( sx, y_+BORDER_WIDTH, swidth, sheight, x_+BORDER_WIDTH, y_+BORDER_WIDTH );
		drawBar(sampleHistoryCount-1, sampleIndex);
	}
	else
	{
		// need to draw entire graph for some reason
		for( i = 0; i < sampleHistoryCount; i++ )
		{
			drawBar(i, (sampleIndex+1+i)%sampleHistoryCount);
		}
	}

	lastWinState = currWinState;
}


void FieldMeterGraph::drawBar(unsigned int column, unsigned int sample)
{
	unsigned int i;
	int start, end;
	double runningtotal = 0.0;
	double total = 0.0;

	for (i=0; i<=numfields_; i++)
	{
		total += heightfield_[sample*(numfields_+1)+i];
	}

	start = 0;
	for (i=0; i<=numfields_; i++)
	{
		runningtotal += heightfield_[sample*(numfields_+1)+i];
		end = floor((height_-2*BORDER_WIDTH)*(runningtotal/total) - 0.5);
		if (end >= start)
		{
			parent_->setForeground(colors_[i]);
			parent_->setStippleN(i%4);
			parent_->drawFilledRectangle(x_+BORDER_WIDTH+column, y_+height_-BORDER_WIDTH-1-end, 1, end-start+1);
			parent_->setStippleN(0);
		}
		start = end+1;
	}
}


void FieldMeterGraph::setNumCols(unsigned int n)
{
	sampleHistoryCount = n;
	sampleIndex = 0;

	// FIXME: This should really allocate the new array; salvage what it
	//   can from the old array; then delete the old array.
	delete [] heightfield_;
	heightfield_ = NULL;
}


void FieldMeterGraph::setNumFields(unsigned int n)
{
	FieldMeterDecay::setNumFields(n);
	delete [] heightfield_;
	heightfield_ = NULL;
}


void FieldMeterGraph::checkResize(void)
{
	unsigned int cols = 0;

	if (x_ == last_x && y_ == last_y && width_ == last_width && height_ == last_height)
		return;

	last_x = x_;
	last_y = y_;
	last_width = width_;
	last_height = height_;

	if (width_ > 2*BORDER_WIDTH)
	{
		cols = width_-2*BORDER_WIDTH;
	}
	setNumCols(cols);
}
