//
//  Copyright (c) 1994, 1995, 2002, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
//
//  This file may be distributed under terms of the GPL
//

#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "xosview.h"
#include "meter.h"
#include "MeterMaker.h"
#if ( defined(XOSVIEW_NETBSD) || defined(XOSVIEW_FREEBSD) || \
      defined(XOSVIEW_OPENBSD) || defined(XOSVIEW_DFBSD) )
# include "kernel.h"
#endif

static const char * const versionString = "xosview version: Git";

static const char NAME[] = "xosview@";

#if !defined(__GNUC__)

#define MIN(x,y)		\
(				\
    x < y ? x : y		\
)

#define MAX(x,y)		\
(				\
    x > y ? x : y		\
)

#else

#define MIN(x,y)		\
({				\
    const typeof(x) _x = x;	\
    const typeof(y) _y = y;	\
				\
    (void) (&_x == &_y);	\
				\
    _x < _y ? _x : _y;		\
})

#define MAX(x,y)		\
({				\
    const typeof(x) _x = x;	\
    const typeof(y) _y = y;	\
				\
    (void) (&_x == &_y);	\
				\
    _x > _y ? _x : _y;		\
})

#endif // sgi

double MAX_SAMPLES_PER_SECOND = 10;

XOSView::XOSView( const char * instName, int argc, char *argv[] ) : XWin(),
						xrm(Xrm("XOSView", instName)){
  // Check for version arguments first.  This allows
  // them to work without the need for a connection
  // to the X server
  checkVersion(argc, argv);

  setDisplayName (xrm.getDisplayName( argc, argv));
  openDisplay();  //  So that the Xrm class can contact the display for its
		  //  default values.
  //  The resources need to be initialized before calling XWinInit, because
  //  XWinInit looks at the geometry resource for its geometry.  BCG
  xrm.loadAndMergeResources (argc, argv, display_);
  XWinInit (argc, argv, NULL, &xrm);
#if 1	//  Don't enable this yet.
  MAX_SAMPLES_PER_SECOND = atof(getResource("samplesPerSec"));
  if (!MAX_SAMPLES_PER_SECOND)
    MAX_SAMPLES_PER_SECOND = 10;
#endif
  usleeptime_ = (unsigned long) (1000000/MAX_SAMPLES_PER_SECOND);
  if (usleeptime_ >= 1000000) {
    /*  The syscall usleep() only takes times less than 1 sec, so
     *  split into a sleep time and a usleep time if needed.  */
    sleeptime_ = usleeptime_ / 1000000;
    usleeptime_ = usleeptime_ % 1000000;
  } else { sleeptime_ = 0; }
#if ( defined(XOSVIEW_NETBSD) || defined(XOSVIEW_FREEBSD) || \
      defined(XOSVIEW_OPENBSD) || defined(XOSVIEW_DFBSD) )
  BSDInit();	/*  Needs to be done before processing of -N option.  */
#endif

  hmargin_  = atoi(getResource("horizontalMargin"));
  vmargin_  = atoi(getResource("verticalMargin"));
  vspacing_ = atoi(getResource("verticalSpacing"));
  hmargin_  = MAX(0, hmargin_);
  vmargin_  = MAX(0, vmargin_);
  vspacing_ = MAX(-BORDER_WIDTH, vspacing_);

  checkArgs (argc, argv);  //  Check for any other unhandled args.
  xoff_ = hmargin_;
  yoff_ = 0;
  nummeters_ = 0;
  meters_ = NULL;
  name_ = const_cast<char *>("xosview");
  _deferred_resize = true;
  _deferred_redraw = true;
  windowVisibility = OBSCURED;

  //  set up the X events
  addEvent( new Event( this, ConfigureNotify,
		      (EventCallBack)&XOSView::resizeEvent ) );
  addEvent( new Event( this, Expose,
		      (EventCallBack)&XOSView::exposeEvent ) );
  addEvent( new Event( this, KeyPress,
		      (EventCallBack)&XOSView::keyPressEvent ) );
  addEvent( new Event( this, VisibilityNotify,
                      (EventCallBack)&XOSView::visibilityEvent ) );
  addEvent( new Event( this, UnmapNotify,
                      (EventCallBack)&XOSView::unmapEvent ) );

  // add or change the Resources
  MeterMaker mm(this);

  // see if legends are to be used
  checkOverallResources ();

  // add in the meters
  mm.makeMeters();
  for (int i = 1 ; i <= mm.n() ; i++)
    addmeter(mm[i]);

  if (nummeters_ == 0)
  {
    fprintf (stderr, "No meters were enabled!  Exiting...\n");
    exit (0);
  }

  //  Have the meters re-check the resources.
  checkMeterResources();

  // determine the width and height of the window then create it
  figureSize();
  init( argc, argv );
  title( winname() );
  iconname( winname() );
  dolegends();
  sampleClock = 0;
}


XOSView::~XOSView( void ){
  MeterNode *tmp = meters_;
  while ( tmp != NULL ){
    MeterNode *save = tmp->next_;
    delete tmp->meter_;
    delete tmp;
    tmp = save;
  }
}


void XOSView::checkVersion(int argc, char *argv[]) const
    {
    for (int i = 0 ; i < argc ; i++)
        if (!strncasecmp(argv[i], "-v", 2)
          || !strncasecmp(argv[i], "--version", 10))
            {
            std::cerr << versionString << std::endl;
            exit(0);
            }
    }

void XOSView::figureSize(void) {
  if ( legend_ ){
    if ( !usedlabels_ )
      xoff_ += textWidth( "XXXXX" );
    else
      xoff_ += textWidth( "XXXXXXXXX" );

    yoff_ = caption_ ? textHeight() : 0;
  }
  static int firsttime = 1;
  if (firsttime) {
    firsttime = 0;
    width_ = 2*hmargin_ + textWidth( "XXXXXXXXXXXXXXXXXXXXXXXXXXXXX" );
    height_ = 2*vmargin_ + nummeters_*(vspacing_+yoff_+textHeight()) - vspacing_;
  }
}

void XOSView::checkMeterResources( void ){
  MeterNode *tmp = meters_;

  while ( tmp != NULL ){
    tmp->meter_->checkResources();
    tmp = tmp->next_;
  }
}

int XOSView::newypos( void ){
  return 15 + 25 * nummeters_;
}

void XOSView::dolegends( void ){
  MeterNode *tmp = meters_;
  while ( tmp != NULL ){
    tmp->meter_->docaptions( caption_ );
    tmp->meter_->dolegends( legend_ );
    tmp->meter_->dousedlegends( usedlabels_ );
    tmp = tmp->next_;
  }
}

void XOSView::addmeter( Meter *fm ){
  MeterNode *tmp = meters_;

  if ( meters_ == NULL )
    meters_ = new MeterNode( fm );
  else {
    while ( tmp->next_ != NULL )
      tmp = tmp->next_;
    tmp->next_ = new MeterNode( fm );
  }
  nummeters_++;
}

void XOSView::checkOverallResources() {
  //  Check various resource values.

  //  Set 'off' value.  This is not necessarily a default value --
  //    the value in the defaultXResourceString is the default value.
  usedlabels_ = legend_ = caption_ = 0;

  setFont();

   // use captions
  if ( isResourceTrue("captions") )
      caption_ = 1;

  // use labels
  if ( isResourceTrue("labels") )
      legend_ = 1;

  // use "free" labels
  if ( isResourceTrue("usedlabels") )
      usedlabels_ = 1;
}

const char *XOSView::winname( void ){
  char host[100];
  gethostname( host, 99 );
  static char name[101];	/*  We return a pointer to this,
				    so it can't be local.  */
  snprintf( name, 100, "%s%s", NAME, host);
  //  Allow overriding of this name through the -title option.
  return getResourceOrUseDefault ("title", name);
}


void XOSView::collect(void) {
  MeterNode *tmp = meters_;

  XOSDEBUG("Doing collect.\n");

  while (tmp != NULL) {
    if (tmp->meter_->requestevent(sampleClock))
      tmp->meter_->checkevent();
    tmp = tmp->next_;
  }
}


void XOSView::history(void) {
  MeterNode *tmp = meters_;

  XOSDEBUG("Doing history.\n");

  while (tmp != NULL) {
    if (tmp->meter_->requestevent(sampleClock))
      tmp->meter_->updateMeterHistory();
    tmp = tmp->next_;
  }
}


void  XOSView::resize(void) {
  int newwidth;
  int newheight;
  int vremain;

  XOSDEBUG("Doing resize.\n");

  newwidth = width_ - xoff_ - hmargin_;
  newwidth = (newwidth > 2*BORDER_WIDTH+1) ? newwidth : 2*BORDER_WIDTH+1;

  newheight = (height_ - (2*vmargin_ + nummeters_*(vspacing_+yoff_) - vspacing_))
      / nummeters_;
  newheight = (newheight > 2*BORDER_WIDTH+1) ? newheight : 2*BORDER_WIDTH+1;

  vremain = (height_ - 2*vmargin_ - nummeters_*(vspacing_+yoff_+newheight) + vspacing_)/2;
  vremain = (vremain > 0) ? vremain : 0;

  int counter = 0;
  MeterNode *tmp = meters_;
  while ( tmp != NULL ) {
    tmp->meter_->resize( xoff_, vmargin_+yoff_+vremain + counter*(newheight+vspacing_+yoff_),
                        newwidth, newheight );
    tmp = tmp->next_;

    counter++;
  }
}


void XOSView::draw(void) {
  MeterNode *tmp = meters_;

  XOSDEBUG("Doing draw.\n");

  clear();

  while (tmp != NULL) {
    tmp->meter_->drawMeterDisplay();
    tmp = tmp->next_;
  }
}


void XOSView::update(void) {
  MeterNode *tmp = meters_;

  XOSDEBUG("Doing update.\n");

  while (tmp != NULL) {
    if (tmp->meter_->requestevent(sampleClock))
      tmp->meter_->updateMeterDisplay();
    tmp = tmp->next_;
  }
}


void XOSView::flushX(bool force) {
  if (!force) {
    MeterNode *tmp = meters_;

    while ( tmp != NULL ){
      if (tmp->meter_->requestevent(sampleClock)) {
        force = true;
        break;
      }
      tmp = tmp->next_;
    }
  }

  if (force) {
    XOSDEBUG("Doing flush.\n");

    flush();
  }
}


void XOSView::run(void) {
  while(!done_) {
    // Update the metrics
    collect();

    // Update meter histories
    // FIXME: This is separate from collect() to reduce sample jitter.
    history();

    // Check for X11 events
    checkevent();

    // Check if the window has been resized (at least once)
    if (_deferred_resize) {
      resize();
      _deferred_resize = false;
      _deferred_redraw = true;
    }

    if (windowVisibility == OBSCURED) {
      _deferred_redraw = false;
    }

    // redraw everything if needed
    if (_deferred_redraw) {
      draw();
    } else {
      // just the updated meters
      update();
    }

    flushX(_deferred_redraw);

    _deferred_redraw = false;

    /*  First, sleep for the proper integral number of seconds --
     *  usleep only deals with times less than 1 sec.  */
    if (sleeptime_) sleep((unsigned int)sleeptime_);
    if (usleeptime_) usleep( (unsigned int)usleeptime_);

    sampleClock++;
  }
}


void XOSView::keyPressEvent( XKeyEvent &event ){
  char c = 0;
  KeySym key;

  XLookupString( &event, &c, 1, &key, NULL );

  if ( (c == 'q') || (c == 'Q') )
    done_ = 1;
}

void XOSView::checkArgs (int argc, char** argv) const
{
  //  The XWin constructor call in the XOSView constructor above
  //  modifies argc and argv, so by this
  //  point, all XResource arguments should be removed.  Since we currently
  //  don't have any other command-line arguments, perform a check here
  //  to make sure we don't get any more.
  if (argc == 1) return;  //  No arguments besides X resources.

  //  Skip to the first real argument.
  argc--;
  argv++;
  while (argc > 0 && argv && *argv)
  {
    switch (argv[0][1]) {
      case 'n': //  Check for -name option that was already parsed
		//  and acted upon by main().
		if (!strncasecmp(*argv, "-name", 6))
		{
		  argv++;	//  Skip arg to -name.
		  argc--;
		}
		break;
#if ( defined(XOSVIEW_NETBSD) || defined(XOSVIEW_FREEBSD) || \
      defined(XOSVIEW_OPENBSD) || defined(XOSVIEW_DFBSD) )
      case 'N': if (strlen(argv[0]) > 2)
		  SetKernelName(argv[0]+2);
		else
		{
		  SetKernelName(argv[1]);
		  argc--;
		  argv++;
		}
		break;
#endif
	      /*  Fall through to default/error case.  */
      default:
		std::cerr << "Ignoring unknown option '" << argv[0] << "'.\n";
		break;
    }
    argc--;
    argv++;
  }
}


void XOSView::exposeEvent(XExposeEvent &event) {
  int i = event.type;	// to quiet the compiler
  i = i;

  _deferred_redraw = true;
  XOSDEBUG("Got expose event.\n");
}


/*
 * All window changes come in via XConfigureEvent (not
 * XResizeRequestEvent)
 */
void XOSView::resizeEvent(XConfigureEvent &event) {
  XOSDEBUG("Got configure event.\n");

  if (event.width == width_ && event.height == height_)
    return;

  XOSDEBUG("Window has resized\n");

  width(event.width);
  height(event.height);
  _deferred_resize = true;
}


void XOSView::visibilityEvent(XVisibilityEvent &event) {
  if (event.state == VisibilityPartiallyObscured) {
    if (windowVisibility != FULLY_VISIBLE)
      _deferred_redraw = true;
    windowVisibility = PARTIALLY_VISIBILE;
  }
  else if (event.state == VisibilityFullyObscured) {
    windowVisibility = OBSCURED;
    _deferred_redraw = false;
  }
  else {
    if (windowVisibility != FULLY_VISIBLE)
      _deferred_redraw = true;
    windowVisibility = FULLY_VISIBLE;
  }

  XOSDEBUG("Got visibility event: %s\n",
    (windowVisibility == FULLY_VISIBLE)
        ? "Full"
        : (windowVisibility == PARTIALLY_VISIBILE)
            ? "Partial"
            : "Obscured"
    );
}


void XOSView::unmapEvent(XUnmapEvent & ev) {
  /* unclutter creates a subwindow of our window if it hides the cursor,
     we get the unmap event if the cursor is moved again. Don't treat it
     as main window unmap */
  if(ev.window == window_)
    windowVisibility = OBSCURED;
}
