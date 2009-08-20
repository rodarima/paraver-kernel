#include "loadedwindows.h"
#include "window.h"
#include "histogram.h"

LoadedWindows *LoadedWindows::instance = NULL;

LoadedWindows::LoadedWindows()
{
  currentID = 0;
  currentHistoID = 0;
}

LoadedWindows *LoadedWindows::getInstance()
{
  if ( LoadedWindows::instance == NULL )
    LoadedWindows::instance = new LoadedWindows();
  return LoadedWindows::instance;
}

LoadedWindows::~LoadedWindows()
{
  for ( map<TWindowID, Window *>::iterator it = windows.begin();
        it != windows.end(); ++it )
    delete ( *it ).second;
  for ( map<TWindowID, Histogram *>::iterator it = histograms.begin();
        it != histograms.end(); ++it )
    delete ( *it ).second;
}

TWindowID LoadedWindows::add( Window *whichWindow )
{
  currentID++;
  windows[ currentID ] = whichWindow;

  return currentID;
}

TWindowID LoadedWindows::add( Histogram *whichHisto )
{
  currentHistoID++;
  histograms[ currentHistoID ] = whichHisto;

  return currentHistoID;
}

Window *LoadedWindows::getWindow( TWindowID id ) const
{
  return windows.find( id )->second;
}

Histogram *LoadedWindows::getHisto( TWindowID id ) const
{
  return histograms.find( id )->second;
}

void LoadedWindows::getAll( vector<Window *>& onVector ) const
{
  for ( map<TWindowID, Window *>::const_iterator it = windows.begin();
        it != windows.end(); ++it )
    onVector.push_back( ( *it ).second );
}

void LoadedWindows::getAll( vector<Histogram *>& onVector ) const
{
  for ( map<TWindowID, Histogram *>::const_iterator it = histograms.begin();
        it != histograms.end(); ++it )
    onVector.push_back( ( *it ).second );
}


void LoadedWindows::getAll( Trace *whichTrace, vector< Window *>& onVector ) const
{
  for ( map<TWindowID, Window *>::const_iterator it = windows.begin();
        it != windows.end(); ++it )
    if ( ( *it ).second->getTrace() == whichTrace )
      onVector.push_back( ( *it ).second );
}


void LoadedWindows::getAll( Trace *whichTrace, vector< Histogram *>& onVector ) const
{
  for ( map<TWindowID, Histogram *>::const_iterator it = histograms.begin();
        it != histograms.end(); ++it )
    if ( ( *it ).second->getControlWindow()->getTrace() == whichTrace )
      onVector.push_back( ( *it ).second );
}


// Histogram windows selection related methods
void LoadedWindows::getValidControlWindow( Window *dataWindow,
    Window *controlWindow,
    vector<TWindowID>& onVector ) const
{
  for ( map<TWindowID, Window *>::const_iterator it = windows.begin();
        it != windows.end(); ++it )
  {
    if ( validDataWindow( dataWindow, ( *it ).second )
         && validDataWindow( controlWindow, ( *it ).second ) )
      onVector.push_back( ( *it ).first );
  }
}

void LoadedWindows::getValidDataWindow( Window *controlWindow,
                                        Window *extraWindow,
                                        vector<TWindowID>& onVector ) const
{
  for ( map<TWindowID, Window *>::const_iterator it = windows.begin();
        it != windows.end(); ++it )
  {
    if ( validDataWindow( ( *it ).second, controlWindow )
         && validDataWindow( ( *it ).second, extraWindow ) )
      onVector.push_back( ( *it ).first );
  }
}

bool LoadedWindows::validDataWindow( Window *dataWindow, Window *controlWindow ) const
{
  if ( dataWindow == controlWindow )
    return true;
  if( controlWindow == NULL )
    return true;
  if( dataWindow == NULL )
    return true;

  else if ( dataWindow->getTrace() == controlWindow->getTrace() )
  {
    if ( validLevelDataWindow( dataWindow, controlWindow ) )
    {
      return notInParents( dataWindow, controlWindow )
             && notInParents( controlWindow, dataWindow );
    }
  }
  else
  {
    // The traces have the same resource and application structure???
    // not yet done
  }

  return false;
}

bool LoadedWindows::validLevelDataWindow( Window *dataWindow, Window *controlWindow ) const
{
  if ( dataWindow->getLevel() == controlWindow->getLevel() )
    return true;
  else
  {
    if ( dataWindow->getLevel() >= WORKLOAD && dataWindow->getLevel() <= THREAD
         &&
         controlWindow->getLevel() >= WORKLOAD && controlWindow->getLevel() <= THREAD )
    {
      if ( dataWindow->getLevel() < controlWindow->getLevel() )
        return true;
    }
    else if ( dataWindow->getLevel() >= SYSTEM && dataWindow->getLevel() <= CPU
              &&
              controlWindow->getLevel() >= SYSTEM && controlWindow->getLevel() <= CPU )
    {
      if ( dataWindow->getLevel() < controlWindow->getLevel() )
        return true;
    }
  }
  return false;
}

bool LoadedWindows::notInParents( Window *whichWindow, Window *inParents ) const
{
  bool result = true;

  if ( whichWindow == inParents )
    result = false;
  else if ( inParents->isDerivedWindow() )
  {
    result = notInParents( whichWindow, inParents->getParent( 0 ) );
    if ( result )
      result = notInParents( whichWindow, inParents->getParent( 1 ) );
  }

  return result;
}
