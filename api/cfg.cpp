/*****************************************************************************\
 *                        ANALYSIS PERFORMANCE TOOLS                         *
 *                               libparaver-api                              *
 *                      API Library for libparaver-kernel                    *
 *****************************************************************************
 *     ___     This library is free software; you can redistribute it and/or *
 *    /  __         modify it under the terms of the GNU LGPL as published   *
 *   /  /  _____    by the Free Software Foundation; either version 2.1      *
 *  /  /  /     \   of the License, or (at your option) any later version.   *
 * (  (  ( B S C )                                                           *
 *  \  \  \_____/   This library is distributed in hope that it will be      *
 *   \  \__         useful but WITHOUT ANY WARRANTY; without even the        *
 *    \___          implied warranty of MERCHANTABILITY or FITNESS FOR A     *
 *                  PARTICULAR PURPOSE. See the GNU LGPL for more details.   *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public License  *
 * along with this library; if not, write to the Free Software Foundation,   *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA          *
 * The GNU LEsser General Public License is contained in the file COPYING.   *
 *                                 ---------                                 *
 *   Barcelona Supercomputing Center - Centro Nacional de Supercomputacion   *
\*****************************************************************************/

#include <math.h>
#include <sstream>
#include <string>
#include <type_traits>

#include "kernelconnection.h"
#include "cfg.h"
#include "window.h"
#include "trace.h"
#include "histogram.h"
#include "histogramstatistic.h"
#include "functionmanagement.h"
#include "semanticfunction.h"
#include "filter.h"
#include "drawmode.h"
#include "symbolpicker.h"
#include "syncwindows.h"
#include "loadedwindows.h"


using namespace std;

// CFG4D
//string TagFunction::currentTag = "";
bool TagFunction::isWindowTag = true;
string TagFunction::currentNameTag = "";

PRV_UINT16 numComposesExtraComposesInWindow = 0;
PRV_UINT16 numSemanticParamExtraComposesInWindow = 0;

map<string, TagFunction *> CFGLoader::cfgTagFunctions;
bool CFGLoader::firstMapLoaded = false;

string currentWindowName;
string CFGLoader::errorLine = "";
bool someEventsExist = false;
bool someEventsNotExist = false;
bool multipleLabelValues = false;
EventTypeSymbolPicker eventTypeSymbolPicker;
EventValueSymbolPicker eventValueSymbolPicker;


// CFGS4D Link data
TCFGS4DIndexLink lastGlobalLinkIndex;

map< TGroupId, TGroupId > syncRealGroup; // Group from CFG -> Group to app
TGroupId lastSyncGroupUsed;

// Drawmodes codification
map<string, DrawModeMethod> drawModeCodes;
map<DrawModeMethod, string> drawModeTags;

// Tags for DrawModeMethod enumeration
// Keep aligned
static const char * drawModeCfgTags[] =
{ "draw_last",
  "draw_maximum",
  "draw_minnotzero",
  "draw_random",
  "draw_randnotzero",
  "draw_average",
  "draw_averagenotzero",
  "draw_mode",
  "draw_absolute_maximum",
  "draw_absolute_minimum",
};

// Before reading cfg
void initDrawModeCodes()
{
  if ( drawModeCodes.size() == 0 )
  {
    // Old codification: "0" --> 0
    // This allows to read old cfgs
    // At the transition moment, DRAW_MODE was the last one
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_LAST ]        = DrawModeMethod::DRAW_LAST;
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_MAXIMUM ]     = DrawModeMethod::DRAW_MAXIMUM;
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_MINNOT0 ]     = DrawModeMethod::DRAW_MINNOTZERO;
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_RANDOM ]      = DrawModeMethod::DRAW_RANDOM;
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_RANDOMNOT0 ]  = DrawModeMethod::DRAW_RANDNOTZERO;
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_AVERAGE ]     = DrawModeMethod::DRAW_AVERAGE;
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_AVERAGENOT0 ] = DrawModeMethod::DRAW_AVERAGENOTZERO;
    drawModeCodes[ OLDCFG_VAL_DRAW_MODE_MODE ]        = DrawModeMethod::DRAW_MODE;
    // New codification: "draw_last" --> 0
    for ( unsigned int current = static_cast<unsigned int>( DrawModeMethod::DRAW_LAST );
          current < static_cast<unsigned int>( DrawModeMethod::DRAW_NUMMETHODS );
          ++current )
    {
      drawModeCodes[ string( drawModeCfgTags[ current ] ) ] = DrawModeMethod( current );
    }
  }
}


// Before writing cfg
void initDrawModeTags()
{
  if ( drawModeTags.size() == 0 )
  {
    // New codification: 0 --> "draw_last"
    for ( unsigned int current = static_cast<unsigned int>( DrawModeMethod::DRAW_LAST );
          current < static_cast<unsigned int>( DrawModeMethod::DRAW_NUMMETHODS );
          ++current )
    {
      drawModeTags[ DrawModeMethod( current ) ] = string( drawModeCfgTags[ current ] );
    }
  }
}


TWindowLevel stringToLevel( const std::string& strLevel )
{
  TWindowLevel level = NONE;

  if ( strLevel.compare( OLDCFG_LVL_APPL ) == 0 )
    level = APPLICATION;
  else if ( strLevel.compare( OLDCFG_LVL_PTASK ) == 0 )
    level = APPLICATION;
  else if ( strLevel.compare( OLDCFG_LVL_WORKLOAD ) == 0 )
    level = WORKLOAD;
  else if ( strLevel.compare( OLDCFG_LVL_TASK ) == 0 )
    level = TASK;
  else if ( strLevel.compare( OLDCFG_LVL_THREAD ) == 0 )
    level = THREAD;
  else if ( strLevel.compare( OLDCFG_LVL_CPU ) == 0 )
    level = CPU;
  else if ( strLevel.compare( OLDCFG_LVL_NODE ) == 0 )
    level = NODE;
  else if ( strLevel.compare( OLDCFG_LVL_SYSTEM ) == 0 )
    level = SYSTEM;
  else if ( strLevel.compare( OLDCFG_LVL_TOPCOMPOSE ) == 0 )
    level = TOPCOMPOSE2;
  else if ( strLevel.compare( OLDCFG_LVL_TOPCOMPOSE1 ) == 0 )
    level = TOPCOMPOSE1;
  else if ( strLevel.compare( OLDCFG_LVL_TOPCOMPOSE2 ) == 0 )
    level = TOPCOMPOSE2;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE_WORKLOAD ) == 0 )
    level = COMPOSEWORKLOAD;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE_APPL ) == 0 )
    level = COMPOSEAPPLICATION;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE_TASK ) == 0 )
    level = COMPOSETASK;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE_THREAD ) == 0 )
    level = COMPOSETHREAD;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE_CPU ) == 0 )
    level = COMPOSECPU;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE_NODE ) == 0 )
    level = COMPOSENODE;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE_SYSTEM ) == 0 )
    level = COMPOSESYSTEM;
  // Old semantic composes
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE1 ) == 0 )
    level = TOPCOMPOSE1;
  else if ( strLevel.compare( OLDCFG_LVL_COMPOSE2 ) == 0 )
    level = TOPCOMPOSE2;
  else if ( strLevel.compare( CFG_LVL_EXTRATOPCOMPOSE1 ) == 0 )
    level = EXTRATOPCOMPOSE1;

  return level;
}


string levelToString( TWindowLevel whichLevel )
{
  switch ( whichLevel )
  {
    case NONE:
    case EXTRATOPCOMPOSE1:
      break;
    case DERIVED:
      break;
    case WORKLOAD:
      return OLDCFG_LVL_WORKLOAD;
      break;
    case APPLICATION:
      return OLDCFG_LVL_APPL;
      break;
    case TASK:
      return OLDCFG_LVL_TASK;
      break;
    case THREAD:
      return OLDCFG_LVL_THREAD;
      break;
    case SYSTEM:
      return OLDCFG_LVL_SYSTEM;
      break;
    case NODE:
      return OLDCFG_LVL_NODE;
      break;
    case CPU:
      return OLDCFG_LVL_CPU;
      break;
    case TOPCOMPOSE1:
      return OLDCFG_LVL_TOPCOMPOSE1;
      break;
    case TOPCOMPOSE2:
      return OLDCFG_LVL_TOPCOMPOSE2;
      break;
    case COMPOSEWORKLOAD:
      return OLDCFG_LVL_COMPOSE_WORKLOAD;
      break;
    case COMPOSEAPPLICATION:
      return OLDCFG_LVL_COMPOSE_APPL;
      break;
    case COMPOSETASK:
      return OLDCFG_LVL_COMPOSE_TASK;
      break;
    case COMPOSETHREAD:
      return OLDCFG_LVL_COMPOSE_THREAD;
      break;
    case COMPOSESYSTEM:
      return OLDCFG_LVL_COMPOSE_SYSTEM;
      break;
    case COMPOSENODE:
      return OLDCFG_LVL_COMPOSE_NODE;
      break;
    case COMPOSECPU:
      return OLDCFG_LVL_COMPOSE_CPU;
      break;
  }

  return "";
}


bool parseSelectedFunctionsNumFunctions( std::istringstream& line, PRV_UINT16& numFunctions )
{
  std::string tmpString;
  std::string strNumFunctions;

  std::getline( line, tmpString, ' ' );
  std::getline( line, strNumFunctions, ',' );
  std::istringstream tmpNumFunctions( strNumFunctions );

  if ( !( tmpNumFunctions >> numFunctions ) )
    return false;

  std::getline( line, tmpString, '{' );

  return true;
}

void parseSelectedFunctionsLevelAndFunction( std::istringstream& line, std::string& strLevel, TWindowLevel& level, std::string& strFunction )
{
  std::string tmpString;

  std::getline( line, tmpString, '{' );
  std::getline( line, strLevel, ',' );
  std::getline( line, tmpString, ' ' );
  std::getline( line, strFunction, '}' );
  level = stringToLevel( strLevel );
}


string levelToStringHisto( TWindowLevel whichLevel )
{
  switch ( whichLevel )
  {
    case WORKLOAD:
      return OLDCFG_LVL_HISTO_WORKLOAD;
      break;
    case APPLICATION:
      return OLDCFG_LVL_HISTO_APPL;
      break;
    case TASK:
      return OLDCFG_LVL_HISTO_TASK;
      break;
    case THREAD:
      return OLDCFG_LVL_HISTO_THREAD;
      break;
    case SYSTEM:
      return OLDCFG_LVL_HISTO_SYSTEM;
      break;
    case NODE:
      return OLDCFG_LVL_HISTO_NODE;
      break;
    case CPU:
      return OLDCFG_LVL_HISTO_CPU;
      break;
    default:
      break;
  }
  return "";
}

void clearSymbolPickers()
{
  eventTypeSymbolPicker.clear();
  eventValueSymbolPicker.clear();
}

bool pickSymbols( Trace *whichTrace, Timeline *whichWindow )
{
  vector<TEventType> tmpTypes;
  vector<TSemanticValue> tmpValues;

  if( !eventTypeSymbolPicker.pick( whichTrace->getEventLabels(), tmpTypes ) )
    return false;
  else
  {
    for( vector<TEventType>::iterator it = tmpTypes.begin(); it != tmpTypes.end(); ++it )
    {
      if ( !whichTrace->eventLoaded( *it ) )
        someEventsNotExist = true;
      else
        someEventsExist = true;

      whichWindow->getFilter()->insertEventType( *it );
    }
  }

  std::vector<std::string> filterFunctions;
  whichWindow->getFilter()->getAllFilterFunctions( filterFunctions );
  if( whichWindow->getFilter()->getEventTypeFunction() == filterFunctions[ 6 ] )
  {
    std::vector<TEventType> rankEvents;
    whichWindow->getFilter()->getEventType( rankEvents );
    if( rankEvents.size() >= 2 && whichTrace->anyEventLoaded( rankEvents[ 0 ], rankEvents[ 1 ] ) )
    {
      someEventsNotExist = false;
      someEventsExist = true;
    }
  }


  if( !eventValueSymbolPicker.pick( whichTrace->getEventLabels(), tmpValues ) )
    return false;
  else
  {
    for( vector<TSemanticValue>::iterator it = tmpValues.begin(); it != tmpValues.end(); ++it )
      whichWindow->getFilter()->insertEventValue( *it );
  }

  multipleLabelValues = eventValueSymbolPicker.getMultipleValuesFound();

  return true;
}


bool CFGLoader::hasCFGExtension( const string& filename )
{
  string cfgExt;

  if ( filename.length() > CFG_SUFFIX.length() )
  {
    // Does it end with ".cfg"?
    cfgExt = filename.substr( filename.length() - CFG_SUFFIX.length() );
    if( cfgExt.compare( CFG_SUFFIX ) == 0 )
      return true;
  }

  return false;
}

// Smarter detections welcome!
bool CFGLoader::isCFGFile( const string& filename )
{
  bool isCFG = false;

  if ( hasCFGExtension( filename ) )
  {
    map< string, bool > found;
    found[ CFG_SHEBANG ] = false;
    found[ CFG_HEADER_VERSION ] = false;
    found[ CFG_HEADER_NUM_WINDOWS ] = false;
    found[ OLDCFG_HEADER_VERSION ] = false;
    found[ OLDCFG_HEADER_NUM_WINDOWS ] = false;
    ifstream cfgFile( filename.c_str() );
    if ( cfgFile.good() )
    {
      while ( !cfgFile.eof() && !isCFG )
      {
        string strLine;
        string cfgHeaderTag;

        getline( cfgFile, strLine );

        if ( strLine.length() > 0 && strLine[ strLine.length() - 1 ] == '\r' )
          strLine = strLine.substr( 0, strLine.length() - 1 );

        if ( strLine.length() > 0 )
        {
          istringstream auxStream( strLine );
          getline( auxStream, cfgHeaderTag, ' ' );

          found[ CFG_SHEBANG ] |= ( cfgHeaderTag.compare( CFG_SHEBANG ) == 0 );
          found[ CFG_HEADER_VERSION ] |= ( cfgHeaderTag.compare( CFG_HEADER_VERSION ) == 0 );
          found[ CFG_HEADER_NUM_WINDOWS ] |= ( cfgHeaderTag.compare( CFG_HEADER_NUM_WINDOWS ) == 0 );
          found[ OLDCFG_HEADER_VERSION ] |= ( cfgHeaderTag.compare( OLDCFG_HEADER_VERSION ) == 0 );
          found[ OLDCFG_HEADER_NUM_WINDOWS ] |= ( cfgHeaderTag.compare( OLDCFG_HEADER_NUM_WINDOWS ) == 0 );

          isCFG = found[ CFG_SHEBANG ] ||
                  ( found[ CFG_HEADER_VERSION ] && found[ CFG_HEADER_NUM_WINDOWS ] ) ||
                  ( found[ OLDCFG_HEADER_VERSION ] && found[ OLDCFG_HEADER_NUM_WINDOWS ] );
        }
      }
    }

    cfgFile.close();
  }

  return isCFG;
}


bool CFGLoader::isDimemasCFGFile( const std::string& filename )
{
  bool isDimemasCFG = false;
  string cfgExt;

  if ( filename.length() > DIMEMAS_CFG_SUFFIX.length() )
  {
    // Does it end with ".cfg"?
    cfgExt = filename.substr( filename.length() - DIMEMAS_CFG_SUFFIX.length() );
    if( cfgExt.compare( DIMEMAS_CFG_SUFFIX ) == 0 )
    {
      // Read shebang
      ifstream cfgFile( filename.c_str() );
      if ( !cfgFile )
        return false;

      if ( !cfgFile.eof() )
      {
        string strLine;

        getline( cfgFile, strLine );

        if ( strLine.length() > 0 && strLine[ strLine.length() - 1 ] == '\r' )
          strLine = strLine.substr( 0, strLine.length() - 1 );

        if ( strLine.length() > 0 )
        {
          isDimemasCFG = ( strLine.compare( DIMEMAS_SHEBANG ) == 0 ) ||
                         ( strLine.compare( OLD_DIMEMAS_SHEBANG ) == 0 );
        }
      }

      cfgFile.close();
    }
  }

  return isDimemasCFG;
}

bool CFGLoader::loadDescription( const std::string& filename, std::string& description )
{
  description = "";
  ifstream cfgFile( filename.c_str() );
  if ( !cfgFile )
    return false;

  bool keepReading = false;
  while ( !cfgFile.eof() )
  {
    std::string strLine;
    std::string cfgTag;

    getline( cfgFile, strLine );

    if ( strLine.length() == 0 )
      continue;

    if ( strLine == CFG_HEADER_END_DESCRIPTION || strLine == OLDCFG_HEADER_END_DESCRIPTION )
    {
      keepReading = false;
      cfgFile.close();
      return description != "";
    }
    if ( keepReading )
      description += strLine + "\n";

    if ( strLine == CFG_HEADER_BEGIN_DESCRIPTION || strLine == OLDCFG_HEADER_BEGIN_DESCRIPTION )
      keepReading = true;
  }
  return false;
}

bool CFGLoader::getCFGTag( ifstream& cfgFile, string& strLine, istringstream& auxStream, string& cfgTag )
{
  getline( cfgFile, strLine );

  if ( strLine.empty() || strLine[ 0 ] == '#' )
    return false;

  if ( strLine.back() == '\r' )
    strLine.pop_back();

  auxStream.str( strLine );

  if ( strLine[ 0 ] == '<' )
    cfgTag = strLine;
  else
    getline( auxStream, cfgTag, ' ' );

  return true;
}


bool CFGLoader::loadCFG( KernelConnection *whichKernel,
                         const string& filename,
                         Trace *whichTrace,
                         vector<Timeline *>& windows,
                         vector<Histogram *>& histograms,
                         SaveOptions &options )
{
  someEventsExist = false;
  someEventsNotExist = false;
  multipleLabelValues = false;

  lastGlobalLinkIndex = 0;

  ifstream cfgFile( filename.c_str() );
  if ( !cfgFile )
    return false;

  if ( !firstMapLoaded ) 
  {
    loadMap();
    firstMapLoaded = true;
  }
  initDrawModeCodes();

  windows.push_back( nullptr );
  histograms.push_back( nullptr );
  options.enabledCFG4DMode = false;
  syncRealGroup.clear();
  CFGLoader::errorLine.clear();

  string strLine;
  string cfgTag;
  istringstream auxStream;
  while ( !cfgFile.eof() )
  {
    auxStream.clear();

    if( !CFGLoader::getCFGTag( cfgFile, strLine, auxStream, cfgTag ) )
      continue;

    if ( strLine.compare( CFG_TAG_CFG4D_ENABLED ) == 0 )
    {
      lastGlobalLinkIndex = CFGS4DGlobalManager::getInstance()->newLinkManager();
      options.enabledCFG4DMode = true;
      continue;
    }

    map<string, TagFunction *>::iterator it = cfgTagFunctions.find( cfgTag );

    if ( it != cfgTagFunctions.end() )
    {
      bool tmpError = false;

      if( windows[ windows.size() - 1 ] != nullptr &&
          !windows[ windows.size() - 1 ]->isDerivedWindow() &&
          typeid( *( it->second ) ) == typeid( WindowName ) ||
          typeid( *( it->second ) ) == typeid( Analyzer2DCreate ) )
      {
        tmpError = !pickSymbols( whichTrace, windows[ windows.size() - 1 ] );
        clearSymbolPickers();
      }

      if ( tmpError ||
           !it->second->parseLine( whichKernel, auxStream, whichTrace, windows,
                                   histograms ) )
      {
        if ( histograms.begin() != histograms.end() &&
             histograms[ histograms.size() - 1 ] != nullptr )
        {
          delete histograms[ histograms.size() - 1 ];
          histograms[ histograms.size() - 1 ] = nullptr;
          CFGLoader::errorLine = strLine;
          break;
        }
        else
        {
          if ( windows[ windows.size() - 1 ] != nullptr )
          {
            delete windows[ windows.size() - 1 ];
            windows[ windows.size() - 1 ] = nullptr;
            CFGLoader::errorLine = strLine;
            break;
          }
        }
      }
    }
  }

  cfgFile.close();

  //unLoadMap();

  if ( histograms[ histograms.size() -1 ] == nullptr )
    histograms.pop_back();

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if( histograms.size() == 0 &&
      !windows[ windows.size() - 1 ]->isDerivedWindow() )
  {
    bool tmpError = !pickSymbols( whichTrace, windows[ windows.size() - 1 ] );
    clearSymbolPickers();
    if( tmpError )
    {
      delete windows[ windows.size() - 1 ];
      //CFGLoader::errorLine = strLine;
      windows[ windows.size() - 1 ] = nullptr;
      return false;
    }
  }

  for( vector<Histogram *>::iterator it = histograms.begin(); it != histograms.end(); ++it )
  {
    if( (*it)->getCFG4DEnabled() )
    {
      map<string, string> tmpAlias( (*it)->getCFG4DStatisticsAliasList() );
      if( tmpAlias.find( (*it)->getCurrentStat() ) == tmpAlias.end() )
        (*it)->setCurrentStat( tmpAlias.begin()->first );
    }
  }

  bool continueLoading = true;
  if ( !someEventsExist )
    continueLoading = whichKernel->userMessage( UserMessageID::MessageCFGNoneEvents );
  else if ( someEventsNotExist )
    continueLoading = whichKernel->userMessage( UserMessageID::MessageCFGSomeEvents );
  else if ( multipleLabelValues )
    continueLoading = whichKernel->userMessage( UserMessageID::MessageCFGMultipleValues );

  // Check if there are some objects in the selected level
  bool someWindowWithSelectedLevelEmpty = false;
  for ( vector<Timeline *>::iterator itWin = windows.begin(); itWin != windows.end(); ++itWin )
  {
    if ( !( *itWin )->hasLevelSomeSelectedObject( ( *itWin )->getLevel() ) )
    {
      someWindowWithSelectedLevelEmpty = true;
      break;
    }
  }
  if ( someWindowWithSelectedLevelEmpty )
    continueLoading = whichKernel->userMessage( UserMessageID::MessageCFGZeroObjects );

  if( !continueLoading )
  {
    for ( vector<Histogram *>::iterator itHisto = histograms.begin(); itHisto != histograms.end(); ++itHisto )
      delete *itHisto;
    histograms.clear();

    for ( vector<Timeline *>::iterator itWin = windows.begin(); itWin != windows.end(); ++itWin )
      delete *itWin;
    windows.clear();

    return true;
  }

  // Init first zoom for all windows
  for ( vector<Timeline *>::iterator it = windows.begin(); it != windows.end(); ++it )
    ( *it )->addZoom( ( *it )->getWindowBeginTime(), ( *it )->getWindowEndTime() );

  // Because old paraver set window_open to false for all windows
  if ( histograms.begin() == histograms.end() )
    windows[ windows.size() - 1 ]->setShowWindow( true );

  // If enabled CFG4D mode, all the windows and histograms must be presented that way.
  if ( options.enabledCFG4DMode )
  {
    for ( vector<Timeline *>::iterator it = windows.begin(); it != windows.end(); ++it )
    {
      ( *it )->setCFG4DEnabled( true );
      ( *it )->setCFG4DMode( true );
      ( *it )->setCFGS4DIndexLink( lastGlobalLinkIndex );
    }

    for ( vector<Histogram *>::iterator it = histograms.begin(); it != histograms.end(); ++it )
    {
      ( *it )->setCFG4DEnabled( true );
      ( *it )->setCFG4DMode( true );
      ( *it )->setCFGS4DIndexLink( lastGlobalLinkIndex );
    }
  }

  return true;
}

void CFGLoader::pushbackWindow( Timeline *whichWindow,
                                vector<Timeline *>& allWindows )
{
  if ( whichWindow->isDerivedWindow() )
  {
    pushbackWindow( whichWindow->getParent( 0 ), allWindows );
    pushbackWindow( whichWindow->getParent( 1 ), allWindows );
  }

  if( whichWindow->isPunctualColorSet() && whichWindow->getPunctualColorWindow() != nullptr )
  {
    if( std::find( allWindows.begin(), allWindows.end(), whichWindow->getPunctualColorWindow() ) == allWindows.end() )
      pushbackWindow( whichWindow->getPunctualColorWindow(), allWindows );
  }

  if( find( allWindows.begin(), allWindows.end(), whichWindow ) == allWindows.end() )
    allWindows.push_back( whichWindow );
}

void CFGLoader::pushbackAllWindows( const vector<Timeline *>& selectedWindows,
                                    const vector<Histogram *>& selectedHistos,
                                    vector<Timeline *>& allWindows )
{
  for ( vector<Timeline *>::const_iterator it = selectedWindows.begin();
        it != selectedWindows.end(); ++it )
  {
    pushbackWindow( ( *it ), allWindows );
  }

  for ( vector<Histogram *>::const_iterator it = selectedHistos.begin();
        it != selectedHistos.end(); ++it )
  {
    // TODO: Consider substitute this section with direct insertions
    if( find( selectedWindows.begin(), selectedWindows.end(), ( *it )->getControlWindow() ) == selectedWindows.end() )
      pushbackWindow( ( *it )->getControlWindow(), allWindows );
    if ( ( *it )->getControlWindow() != ( *it )->getDataWindow() &&
         find( selectedWindows.begin(), selectedWindows.end(), ( *it )->getDataWindow() ) == selectedWindows.end() )
      pushbackWindow( ( *it )->getDataWindow(), allWindows );
    if ( ( *it )->getThreeDimensions() &&
         ( *it )->getExtraControlWindow() != ( *it )->getControlWindow() &&
         ( *it )->getExtraControlWindow() != ( *it )->getDataWindow() &&
         find( selectedWindows.begin(), selectedWindows.end(), ( *it )->getExtraControlWindow() ) == selectedWindows.end() )
      pushbackWindow( ( *it )->getExtraControlWindow(), allWindows );
  }
}

bool CFGLoader::saveCFG( const string& filename,
                         const SaveOptions& options,
                         const vector<Timeline *>& windows,
                         const vector<Histogram *>& histograms,
                         const vector<CFGS4DLinkedPropertiesManager>& linkedProperties )
{
  vector<Timeline *> allWindows;

  ofstream cfgFile( filename.c_str() );
  if ( !cfgFile )
    return false;

  syncRealGroup.clear();
  lastSyncGroupUsed = 0;

  initDrawModeTags();

  pushbackAllWindows( windows, histograms, allWindows );

  cfgFile << fixed;
  cfgFile.precision( 12 );

  cfgFile << CFG_SHEBANG << endl;
  //cfgFile << "ConfigFile.Version: 3.4" << endl;
  //cfgFile << "ConfigFile.NumWindows: " << allWindows.size() << endl;
  cfgFile << CFG_HEADER_VERSION << " " << CFG_CURRENT_VERSION << endl;
  cfgFile << CFG_HEADER_NUM_WINDOWS << " " << allWindows.size() << endl;
  cfgFile << CFG_HEADER_BEGIN_DESCRIPTION << endl;
  cfgFile << options.description << endl;
  cfgFile << CFG_HEADER_END_DESCRIPTION << endl;

  if ( options.enabledCFG4DMode )
    cfgFile << CFG_TAG_CFG4D_ENABLED << endl;
//    TagEnabledCFG4D::printLine( cfgFile, it );

  cfgFile << endl;

  int id = 1;

  for ( vector<Timeline *>::const_iterator it = allWindows.begin();
        it != allWindows.end(); ++it )
  {
    cfgFile << "################################################################################" << endl;
    cfgFile << "< NEW DISPLAYING WINDOW " << ( *it )->getName() << " >" << endl;
    cfgFile << "################################################################################" << endl;
    WindowName::printLine( cfgFile, it );
    WindowType::printLine( cfgFile, it );
    cfgFile << OLDCFG_TAG_WNDW_ID << " " << id << endl;
    if ( ( *it )->isDerivedWindow() )
    {
      WindowFactors::printLine( cfgFile, it );
      WindowShifts::printLine( cfgFile, it );
      WindowOperation::printLine( cfgFile, it );
      WindowIdentifiers::printLine( cfgFile, allWindows, it );
    }
    WindowPositionX::printLine( cfgFile, it );
    WindowPositionY::printLine( cfgFile, it );
    WindowWidth::printLine( cfgFile, it );
    WindowHeight::printLine( cfgFile, it );
    WindowCommLines::printLine( cfgFile, it );
    WindowFlagsEnabled::printLine( cfgFile, it );
    WindowNonColorMode::printLine( cfgFile, it );
    WindowColorMode::printLine( cfgFile, it );
    WindowCustomColorEnabled::printLine( cfgFile, it );
    WindowCustomColorPalette::printLine( cfgFile, it );
    WindowSemanticScaleMinAtZero::printLine( cfgFile, it );
    WindowPunctualColorWindow::printLine( cfgFile, allWindows, it );
    if ( !( *it )->isDerivedWindow() )
    {
      WindowFilterLogical::printLine( cfgFile, it );
      WindowFilterPhysical::printLine( cfgFile, it );
      WindowFilterBoolOpFromTo::printLine( cfgFile, it );
      WindowFilterBoolOpTagSize::printLine( cfgFile, it );
      WindowFilterBoolOpTypeVal::printLine( cfgFile, it );
    }
    WindowUnits::printLine( cfgFile, it );
    WindowMaximumY::printLine( cfgFile, it );
    WindowMinimumY::printLine( cfgFile, it );
    WindowComputeYMax::printLine( cfgFile, options, it );
    WindowLevel::printLine( cfgFile, it );
    WindowZoomObjects::printLine( cfgFile, it );
    WindowScaleRelative::printLine( cfgFile, options, it );
    WindowEndTimeRelative::printLine( cfgFile, options, it );
    WindowObject::printLine( cfgFile, it );
    WindowBeginTime::printLine( cfgFile, options, it );
    WindowEndTime::printLine( cfgFile, options, it );
    WindowStopTime::printLine( cfgFile, options, it );
    WindowBeginTimeRelative::printLine( cfgFile, options, it );
    WindowOpen::printLine( cfgFile, it );
    WindowDrawMode::printLine( cfgFile, it );
    WindowDrawModeRows::printLine( cfgFile, it );
    WindowPixelSize::printLine( cfgFile, it );
    WindowLabelsToDraw::printLine( cfgFile, it );
    WindowSelectedFunctions::printLine( cfgFile, it );
    WindowComposeFunctions::printLine( cfgFile, it );
    WindowSemanticModule::printLine( cfgFile, it );
    if ( !( *it )->isDerivedWindow() )
      WindowFilterModule::printLine( cfgFile, it );
    if( ( *it )->isSync() )
      WindowSynchronize::printLine( cfgFile, it );

    if ( options.enabledCFG4DMode )
    {
      cfgFile << endl;
      TagAliasCFG4D::printAliasList( cfgFile, it );
      TagAliasParamCFG4D::printAliasList( cfgFile, it );
      TagLinkCFG4D::printLinkList( cfgFile, linkedProperties[ 0 ], *it );
    }

    cfgFile << endl;
    ++id;
  }

  for ( vector<Histogram *>::const_iterator it = histograms.begin();
        it != histograms.end(); ++it )
  {
    Analyzer2DCreate::printLine( cfgFile, it );
    Analyzer2DName::printLine( cfgFile, it );
    Analyzer2DX::printLine( cfgFile, it );
    Analyzer2DY::printLine( cfgFile, it );
    Analyzer2DWidth::printLine( cfgFile, it );
    Analyzer2DHeight::printLine( cfgFile, it );
    Analyzer2DControlWindow::printLine( cfgFile, allWindows, it );
    Analyzer2DDataWindow::printLine( cfgFile, allWindows, it );
    Analyzer2DAccumulator::printLine( cfgFile, it );
    Analyzer2DStatistic::printLine( cfgFile, it );
    Analyzer2DCalculateAll::printLine( cfgFile, it );
    Analyzer2DHideColumns::printLine( cfgFile, it );
    Analyzer2DHorizontal::printLine( cfgFile, it );
    Analyzer2DColor::printLine( cfgFile, it );
    Analyzer2DSemanticColor::printLine( cfgFile, it );
    Analyzer2DZoom::printLine( cfgFile, it );
    Analyzer2DAccumulateByControlWindow::printLine( cfgFile, it );
    Analyzer2DSortCols::printLine( cfgFile, it );
    Analyzer2DSortCriteria::printLine( cfgFile, it );
    Analyzer2DSortReverse::printLine( cfgFile, it );
    Analyzer2DParameters::printLine( cfgFile, it );
    Analyzer2DAnalysisLimits::printLine( cfgFile, options, it );
    Analyzer2DRelativeTime::printLine( cfgFile, it );
    Analyzer2DComputeYScale::printLine( cfgFile, options, it );
    Analyzer2DComputeYScaleZero::printLine( cfgFile, options, it );
    Analyzer2DMinimum::printLine( cfgFile, it );
    Analyzer2DMaximum::printLine( cfgFile, it );
    Analyzer2DDelta::printLine( cfgFile, it );
    Analyzer2DUseCustomDelta::printLine( cfgFile, it );
    Analyzer2DNumColumns::printLine( cfgFile, it );
    Analyzer2DComputeGradient::printLine( cfgFile, options, it );
    Analyzer2DMinimumGradient::printLine( cfgFile, it );
    Analyzer2DMaximumGradient::printLine( cfgFile, it );
    Analyzer2DObjects::printLine( cfgFile, it );
    Analyzer2DDrawModeObjects::printLine( cfgFile, it );
    Analyzer2DDrawModeColumns::printLine( cfgFile, it );
    Analyzer2DPixelSize::printLine( cfgFile, it );
    Analyzer2DColorMode::printLine( cfgFile, it );
    Analyzer2DOnlyTotals::printLine( cfgFile, it );
    Analyzer2DShortLabels::printLine( cfgFile, it );
    if( ( *it )->isSync() )
      Analyzer2DSynchronize::printLine( cfgFile, it );

    if ( ( *it )->getThreeDimensions() )
    {
      Analyzer3DControlWindow::printLine( cfgFile, allWindows, it );
      Analyzer3DComputeYScale::printLine( cfgFile, options, it );
      Analyzer3DMinimum::printLine( cfgFile, it );
      Analyzer3DMaximum::printLine( cfgFile, it );
      Analyzer3DDelta::printLine( cfgFile, it );
      Analyzer3DFixedValue::printLine( cfgFile, it );
    }

    if ( options.enabledCFG4DMode )
    {
      cfgFile << endl;
      TagAliasCFG4D::printAliasList( cfgFile, it );
      TagAliasStatisticCFG4D::printAliasList( cfgFile, it );
      TagLinkCFG4D::printLinkList( cfgFile, linkedProperties[ 0 ], *it );
    }
    cfgFile << endl;
  }

  cfgFile.close();

  return true;
}

int CFGLoader::findWindow( const Timeline *whichWindow,
                           const vector<Timeline *>& allWindows )
{
  unsigned int i = 0;

  if ( allWindows.begin() == allWindows.end() )
    return -1;

  while ( i < allWindows.size() )
  {
    if ( whichWindow == allWindows[ i ] )
      break;
    ++i;
  }

  if ( i == allWindows.size() )
    return -1;

  return i;
}


int CFGLoader::findWindowBackwards( const Timeline *whichWindow,
                                    const vector<Timeline *>& allWindows,
                                    const vector<Timeline *>::const_iterator it )
{
  int i = std::distance( allWindows.begin(), it );

/*  for( vector<Timeline *>::iterator tmpIt = it; it != allWindows.begin(); --it )
    ++i;*/
  if ( allWindows.begin() == allWindows.end() )
    return -1;

  --i;
  while ( i >= 0 )
  {
    if ( whichWindow == allWindows[ i ] )
      break;
    --i;
  }

  return i;
}


void CFGLoader::loadMap()
{
  cfgTagFunctions[OLDCFG_TAG_WNDW_NAME]                = new WindowName();
  cfgTagFunctions[OLDCFG_TAG_WNDW_TYPE]                = new WindowType();
  cfgTagFunctions[OLDCFG_TAG_WNDW_FACTORS]             = new WindowFactors();
  cfgTagFunctions[CFG_TAG_WNDW_SHIFTS]                 = new WindowShifts();
  cfgTagFunctions[OLDCFG_TAG_WNDW_POSX]                = new WindowPositionX();
  cfgTagFunctions[OLDCFG_TAG_WNDW_POSY]                = new WindowPositionY();
  cfgTagFunctions[OLDCFG_TAG_WNDW_WIDTH]               = new WindowWidth();
  cfgTagFunctions[OLDCFG_TAG_WNDW_HEIGHT]              = new WindowHeight();
  cfgTagFunctions[OLDCFG_TAG_WNDW_COMM_LINES]          = new WindowCommLines();
  cfgTagFunctions[OLDCFG_TAG_WNDW_FLAGS_ENABLED]       = new WindowFlagsEnabled();
  cfgTagFunctions[OLDCFG_TAG_WNDW_NON_COLOR_MODE]      = new WindowNonColorMode();
  cfgTagFunctions[OLDCFG_TAG_WNDW_UNITS]               = new WindowUnits();
  cfgTagFunctions[OLDCFG_TAG_WNDW_COLOR_MODE]          = new WindowColorMode();
  // Color palette
  cfgTagFunctions[OLDCFG_TAG_WNDW_CUSTOM_COLOR_ENABLED] = new WindowCustomColorEnabled();
  cfgTagFunctions[OLDCFG_TAG_WNDW_CUSTOM_COLOR_PALETTE] = new WindowCustomColorPalette();
  cfgTagFunctions[OLDCFG_TAG_WNDW_SEMANTIC_SCALE_MIN_AT_ZERO] = new WindowSemanticScaleMinAtZero();
  cfgTagFunctions[OLDCFG_TAG_WNDW_OPERATION]           = new WindowOperation();
  cfgTagFunctions[OLDCFG_TAG_WNDW_MAXIMUM_Y]           = new WindowMaximumY();
  cfgTagFunctions[OLDCFG_TAG_WNDW_MINIMUM_Y]           = new WindowMinimumY();
  cfgTagFunctions[OLDCFG_TAG_WNDW_COMPUTE_Y_MAX]       = new WindowComputeYMax();
  cfgTagFunctions[OLDCFG_TAG_WNDW_LEVEL]               = new WindowLevel();
  cfgTagFunctions[OLDCFG_TAG_WNDW_SCALE_RELATIVE]      = new WindowScaleRelative();
  cfgTagFunctions[CFG_TAG_WNDW_END_TIME_RELATIVE]      = new WindowEndTimeRelative();
  cfgTagFunctions[OLDCFG_TAG_WNDW_OBJECT]              = new WindowObject();
  cfgTagFunctions[OLDCFG_TAG_WNDW_IDENTIFIERS]         = new WindowIdentifiers();

  cfgTagFunctions[OLDCFG_TAG_WNDW_ZOOM_OBJECTS]        = new WindowZoomObjects();
  cfgTagFunctions[OLDCFG_TAG_WNDW_BEGIN_TIME]          = new WindowBeginTime();
  cfgTagFunctions[OLDCFG_TAG_WNDW_STOP_TIME]           = new WindowStopTime();
  cfgTagFunctions[OLDCFG_TAG_WNDW_END_TIME]            = new WindowEndTime();
  cfgTagFunctions[OLDCFG_TAG_WNDW_BEGIN_TIME_RELATIVE] = new WindowBeginTimeRelative();

  cfgTagFunctions[OLDCFG_TAG_WNDW_NUMBER_OF_ROWS]      = new WindowNumberOfRow();
  cfgTagFunctions[OLDCFG_TAG_WNDW_SELECTED_FUNCTIONS]  = new WindowSelectedFunctions();
  cfgTagFunctions[OLDCFG_TAG_WNDW_SEMANTIC_MODULE]     = new WindowSemanticModule();
  cfgTagFunctions[OLDCFG_TAG_WNDW_COMPOSE_FUNCTIONS]   = new WindowComposeFunctions();

  // Filter options
  cfgTagFunctions[OLDCFG_TAG_WNDW_FILTER_MODULE]       = new WindowFilterModule();
  cfgTagFunctions[OLDCFG_TAG_WNDW_LOGICAL_FILTERED]    = new WindowFilterLogical();
  cfgTagFunctions[OLDCFG_TAG_WNDW_PHYSICAL_FILTERED]   = new WindowFilterPhysical();
  cfgTagFunctions[OLDCFG_TAG_WNDW_FROMTO]              = new WindowFilterBoolOpFromTo();
  cfgTagFunctions[OLDCFG_TAG_WNDW_COMM_TAGSIZE]        = new WindowFilterBoolOpTagSize();
  cfgTagFunctions[OLDCFG_TAG_WNDW_TYPEVAL]             = new WindowFilterBoolOpTypeVal();

  cfgTagFunctions[OLDCFG_TAG_WNDW_OPEN]                = new WindowOpen();
  cfgTagFunctions[OLDCFG_TAG_WNDW_DRAW_MODE]           = new WindowDrawMode();
  cfgTagFunctions[OLDCFG_TAG_WNDW_DRAW_MODE_ROWS]      = new WindowDrawModeRows();
  cfgTagFunctions[OLDCFG_TAG_WNDW_PIXEL_SIZE]          = new WindowPixelSize();
  cfgTagFunctions[OLDCFG_TAG_WNDW_LABELS_TO_DRAW]      = new WindowLabelsToDraw();
  cfgTagFunctions[OLDCFG_TAG_WNDW_PUNCTUAL_COLOR_WIN]  = new WindowPunctualColorWindow();
  cfgTagFunctions[OLDCFG_TAG_WNDW_SYNCHRONIZE]         = new WindowSynchronize();

  // Histogram options

  cfgTagFunctions[OLDCFG_TAG_AN2D_NEW]                 = new Analyzer2DCreate();
  cfgTagFunctions[OLDCFG_TAG_AN2D_NAME]                = new Analyzer2DName();
  cfgTagFunctions[OLDCFG_TAG_AN2D_X]                   = new Analyzer2DX();
  cfgTagFunctions[OLDCFG_TAG_AN2D_Y]                   = new Analyzer2DY();
  cfgTagFunctions[OLDCFG_TAG_AN2D_WIDTH]               = new Analyzer2DWidth();
  cfgTagFunctions[OLDCFG_TAG_AN2D_HEIGHT]              = new Analyzer2DHeight();
  cfgTagFunctions[OLDCFG_TAG_AN2D_CONTROL_WINDOW]      = new Analyzer2DControlWindow();
  cfgTagFunctions[OLDCFG_TAG_AN2D_DATA_WINDOW]         = new Analyzer2DDataWindow();
  cfgTagFunctions[OLDCFG_TAG_AN2D_STATISTIC]           = new Analyzer2DStatistic();
  cfgTagFunctions[OLDCFG_TAG_AN2D_CALCULATE_ALL]       = new Analyzer2DCalculateAll();
  // --> Analyzer2D.hide_empty_cols:
  cfgTagFunctions[OLDCFG_TAG_AN2D_HIDE_COLS]           = new Analyzer2DHideColumns();
  cfgTagFunctions[OLDCFG_TAG_AN2D_HORIZONTAL]          = new Analyzer2DHorizontal();
  cfgTagFunctions[OLDCFG_TAG_AN2D_COLOR]               = new Analyzer2DColor();
  cfgTagFunctions[OLDCFG_TAG_AN2D_SEMANTIC_COLOR]      = new Analyzer2DSemanticColor();
  // --> Analyzer2D.TextMode:
  cfgTagFunctions[OLDCFG_TAG_AN2D_ZOOM]                = new Analyzer2DZoom();
  // --> Analyzer2D.Expanded:
  // --> Analyzer2D.expanded:
  cfgTagFunctions[OLDCFG_TAG_AN2D_ACCUMULATOR]          = new Analyzer2DAccumulator();
  cfgTagFunctions[OLDCFG_TAG_AN2D_ACCUM_BY_CTRL_WINDOW] = new Analyzer2DAccumulateByControlWindow();
  cfgTagFunctions[OLDCFG_TAG_AN2D_SORTCOLS]             = new Analyzer2DSortCols();
  cfgTagFunctions[OLDCFG_TAG_AN2D_SORTCRITERIA]         = new Analyzer2DSortCriteria();
  cfgTagFunctions[OLDCFG_TAG_AN2D_SORTREVERSE]         = new Analyzer2DSortReverse();

  cfgTagFunctions[OLDCFG_TAG_AN2D_PARAMETERS]           = new Analyzer2DParameters();
  cfgTagFunctions[OLDCFG_TAG_AN2D_ANALYSISLIMITS]       = new Analyzer2DAnalysisLimits();
  cfgTagFunctions[OLDCFG_TAG_AN2D_RELATIVETIME]         = new Analyzer2DRelativeTime();
  // --> cfgTagFunctions["Analyzer2D.RelativeXScale:"] = new Analyzer2DRelativeXScale();
  // --> Analyzer2D.ShowWindows:
  cfgTagFunctions[OLDCFG_TAG_AN2D_COMPUTEYSCALE]        = new Analyzer2DComputeYScale();
  cfgTagFunctions[CFG_TAG_AN2D_COMPUTEYSCALE_ZERO]      = new Analyzer2DComputeYScaleZero();
  cfgTagFunctions[OLDCFG_TAG_AN2D_MINIMUM]              = new Analyzer2DMinimum();
  cfgTagFunctions[OLDCFG_TAG_AN2D_MAXIMUM]              = new Analyzer2DMaximum();
  cfgTagFunctions[OLDCFG_TAG_AN2D_DELTA]                = new Analyzer2DDelta();
  cfgTagFunctions[CFG_TAG_AN2D_USE_CUSTOM_DELTA]        = new Analyzer2DUseCustomDelta();
  cfgTagFunctions[CFG_TAG_AN2D_NUMCOLUMNS]              = new Analyzer2DNumColumns();
  cfgTagFunctions[OLDCFG_TAG_AN2D_COMPUTEGRADIENT]      = new Analyzer2DComputeGradient();
  cfgTagFunctions[OLDCFG_TAG_AN2D_MINIMUMGRADIENT]      = new Analyzer2DMinimumGradient();
  cfgTagFunctions[OLDCFG_TAG_AN2D_MAXIMUMGRADIENT]      = new Analyzer2DMaximumGradient();
  cfgTagFunctions[ CFG_TAG_OBJECTS ]                    = new Analyzer2DObjects();
  cfgTagFunctions[ CFG_TAG_DRAWMODE_OBJECTS ]           = new Analyzer2DDrawModeObjects();
  cfgTagFunctions[ CFG_TAG_DRAWMODE_COLUMNS ]           = new Analyzer2DDrawModeColumns();
  cfgTagFunctions[OLDCFG_TAG_AN2D_PIXEL_SIZE]           = new Analyzer2DPixelSize();
  cfgTagFunctions[OLDCFG_TAG_AN2D_CODE_COLOR]           = new Analyzer2DCodeColor();
  cfgTagFunctions[OLDCFG_TAG_AN2D_COLOR_MODE]           = new Analyzer2DColorMode();
  cfgTagFunctions[OLDCFG_TAG_AN2D_ONLY_TOTALS]          = new Analyzer2DOnlyTotals();
  cfgTagFunctions[OLDCFG_TAG_AN2D_SHORT_LABELS]         = new Analyzer2DShortLabels();
  cfgTagFunctions[OLDCFG_TAG_AN2D_SYNCHRONIZE]          = new Analyzer2DSynchronize();

  // 3D Histogram
  cfgTagFunctions[OLDCFG_TAG_AN3D_CONTROLWINDOW]        = new Analyzer3DControlWindow();
  cfgTagFunctions[OLDCFG_TAG_AN3D_COMPUTEYSCALE]        = new Analyzer3DComputeYScale();
  cfgTagFunctions[OLDCFG_TAG_AN3D_MINIMUM]              = new Analyzer3DMinimum();
  cfgTagFunctions[OLDCFG_TAG_AN3D_MAXIMUM]              = new Analyzer3DMaximum();
  cfgTagFunctions[OLDCFG_TAG_AN3D_DELTA]                = new Analyzer3DDelta();
  cfgTagFunctions[OLDCFG_TAG_AN3D_FIXEDVALUE]           = new Analyzer3DFixedValue();

  cfgTagFunctions[ CFG_TAG_ALIAS_CFG4D ]                = new TagAliasCFG4D();
  cfgTagFunctions[ CFG_TAG_LINK_CFG4D ]                 = new TagLinkCFG4D();
  cfgTagFunctions[ CFG_TAG_STATISTIC_ALIAS_CFG4D ]      = new TagAliasStatisticCFG4D();
  cfgTagFunctions[ CFG_TAG_PARAM_ALIAS_CFG4D ]          = new TagAliasParamCFG4D();
}

void CFGLoader::unLoadMap()
{
  for ( map<string, TagFunction *>::iterator it = cfgTagFunctions.begin();
        it != cfgTagFunctions.end();
        ++it )
    delete ( *it ).second;
}


string WindowName::tagCFG = OLDCFG_TAG_WNDW_NAME;

bool WindowName::parseLine( KernelConnection *whichKernel, istringstream& line,
                            Trace *whichTrace,
                            vector<Timeline *>& windows,
                            vector<Histogram *>& histograms )
{
  string name;

  getline( line, name );

  currentWindowName = name;

  isWindowTag = true; // CFG4D

  numComposesExtraComposesInWindow = 0;
  numSemanticParamExtraComposesInWindow = 0;

  return true;
}

void WindowName::printLine( ofstream& cfgFile,
                            const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_NAME << " " << ( *it )->getName() << endl;
}


string WindowType::tagCFG = OLDCFG_TAG_WNDW_TYPE;

bool WindowType::parseLine( KernelConnection *whichKernel, istringstream& line,
                            Trace *whichTrace,
                            vector<Timeline *>& windows,
                            vector<Histogram *>& histograms )
{
  string type;
  Timeline *tmpWin;

  isWindowTag = true; // CFG4D

  getline( line, type );
  if ( type.compare( OLDCFG_VAL_WNDW_TYPE_SINGLE ) == 0 )
  {
    tmpWin = Timeline::create( whichKernel, whichTrace );
  }
  else if ( type.compare( OLDCFG_VAL_WNDW_TYPE_COMPOSED ) == 0 )
  {
    tmpWin = Timeline::create( whichKernel );
  }
  else
    return false;

  // Force Code Color: cfgs assume this by default
  tmpWin->setCodeColorMode();

  if ( windows.size() == 1 && windows[ 0 ] == nullptr )
    windows[ 0 ] = tmpWin;
  else
    windows.push_back( tmpWin );

  windows[ windows.size() - 1 ]->setName( currentWindowName );

  // Default value from old paraver code
  windows[ windows.size() - 1 ]->setTimeUnit( US );

  // CFG4D
  if ( currentNameTag.compare( "" ) != 0 )
  {
    windows[ windows.size() - 1 ]->setCFG4DAlias( OLDCFG_TAG_WNDW_NAME, currentNameTag );
    setCurrentNameTag( "" );
  }
  else
  {
    windows[ windows.size() - 1 ]->setCFG4DMode( false );
  }

  return true;
}

void WindowType::printLine( ofstream& cfgFile,
                            const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_TYPE << " ";
  if ( ( *it )->isDerivedWindow() )
    cfgFile << OLDCFG_VAL_WNDW_TYPE_COMPOSED << endl;
  else
    cfgFile << OLDCFG_VAL_WNDW_TYPE_SINGLE << endl;
}



string WindowFactors::tagCFG = OLDCFG_TAG_WNDW_FACTORS;

bool WindowFactors::parseLine( KernelConnection *whichKernel, istringstream& line,
                               Trace *whichTrace,
                               vector<Timeline *>& windows,
                               vector<Histogram *>& histograms )
{
  string strFactor;
  PRV_UINT16 numFactor = 0;

  isWindowTag = true; // CFG4D

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( !windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  while ( !line.eof() )
  {
    getline( line, strFactor, ' ' );
    istringstream tmpStream( strFactor );
    TSemanticValue factorValue;

    if ( !( tmpStream >> factorValue ) )
      return false;

    windows[ windows.size() - 1 ]->setFactor( numFactor, factorValue );

    numFactor++;
  }
  return true;
}

void WindowFactors::printLine( ofstream& cfgFile,
                               const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_FACTORS << " " << ( *it )->getFactor( 0 ) <<
  " " << ( *it )->getFactor( 1 ) << endl;
}


string WindowShifts::tagCFG = CFG_TAG_WNDW_SHIFTS;

bool WindowShifts::parseLine( KernelConnection *whichKernel, istringstream& line,
                              Trace *whichTrace,
                              vector<Timeline *>& windows,
                              vector<Histogram *>& histograms )
{
  string strShift;
  PRV_UINT16 numShift = 0;

  isWindowTag = true; // CFG4D

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( !windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  while ( !line.eof() )
  {
    getline( line, strShift, ' ' );
    istringstream tmpStream( strShift );
    PRV_INT16 shiftValue;

    if ( !( tmpStream >> shiftValue ) )
      return false;

    windows[ windows.size() - 1 ]->setShift( numShift, shiftValue );

    ++numShift;
  }
  return true;
}

void WindowShifts::printLine( ofstream& cfgFile,
                              const vector<Timeline *>::const_iterator it )
{
  cfgFile << CFG_TAG_WNDW_SHIFTS << " " << ( *it )->getShift( 0 ) <<
    " " << ( *it )->getShift( 1 ) << endl;
}


string WindowPositionX::tagCFG = OLDCFG_TAG_WNDW_POSX;

bool WindowPositionX::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strPos;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strPos, ' ' );
  istringstream tmpStream( strPos );
  PRV_UINT16 pos;

  if ( !( tmpStream >> pos ) )
    return false;

  windows[ windows.size() - 1 ]->setPosX( pos );

  return true;
}

void WindowPositionX::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_POSX << " " << ( *it )->getPosX() << endl;
}



string WindowPositionY::tagCFG = OLDCFG_TAG_WNDW_POSY;

bool WindowPositionY::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strPos;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strPos, ' ' );
  istringstream tmpStream( strPos );
  PRV_UINT16 pos;

  if ( !( tmpStream >> pos ) )
    return false;

  windows[ windows.size() - 1 ]->setPosY( pos );

  return true;
}

void WindowPositionY::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_POSY << " " << ( *it )->getPosY() << endl;
}



string WindowWidth::tagCFG = OLDCFG_TAG_WNDW_WIDTH;

bool WindowWidth::parseLine( KernelConnection *whichKernel, istringstream& line,
                             Trace *whichTrace,
                             vector<Timeline *>& windows,
                             vector<Histogram *>& histograms )
{
  string strWidth;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strWidth, ' ' );
  istringstream tmpStream( strWidth );
  PRV_UINT16 width;

  if ( !( tmpStream >> width ) )
    return false;

  windows[ windows.size() - 1 ]->setWidth( width );

  return true;
}

void WindowWidth::printLine( ofstream& cfgFile,
                             const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_WIDTH << " " << ( *it )->getWidth() << endl;
}



string WindowHeight::tagCFG = OLDCFG_TAG_WNDW_HEIGHT;

bool WindowHeight::parseLine( KernelConnection *whichKernel, istringstream& line,
                              Trace *whichTrace,
                              vector<Timeline *>& windows,
                              vector<Histogram *>& histograms )
{
  string strHeight;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strHeight, ' ' );
  istringstream tmpStream( strHeight );
  PRV_UINT16 height;

  if ( !( tmpStream >> height ) )
    return false;

  windows[ windows.size() - 1 ]->setHeight( height );

  return true;
}

void WindowHeight::printLine( ofstream& cfgFile,
                              const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_HEIGHT << " " << ( *it )->getHeight() << endl;
}



string WindowCommLines::tagCFG = OLDCFG_TAG_WNDW_COMM_LINES;

bool WindowCommLines::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    windows[ windows.size() - 1 ]->setDrawCommLines( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    windows[ windows.size() - 1 ]->setDrawCommLines( true );
  else
    return false;

  return true;
}

void WindowCommLines::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_COMM_LINES << " " << ( ( *it )->getDrawCommLines() ?
      OLDCFG_VAL_TRUE : OLDCFG_VAL_FALSE ) << endl;
}



string WindowFlagsEnabled::tagCFG = OLDCFG_TAG_WNDW_FLAGS_ENABLED;

bool WindowFlagsEnabled::parseLine( KernelConnection *whichKernel, istringstream& line,
                                    Trace *whichTrace,
                                    vector<Timeline *>& windows,
                                    vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    windows[ windows.size() - 1 ]->setDrawFlags( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    windows[ windows.size() - 1 ]->setDrawFlags( true );
  else
    return false;

  return true;
}

void WindowFlagsEnabled::printLine( ofstream& cfgFile,
                                    const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_FLAGS_ENABLED << " " << ( ( *it )->getDrawFlags() ?
      OLDCFG_VAL_TRUE : OLDCFG_VAL_FALSE ) << endl;
}



string WindowNonColorMode::tagCFG = OLDCFG_TAG_WNDW_NON_COLOR_MODE;

bool WindowNonColorMode::parseLine( KernelConnection *whichKernel, istringstream& line,
                                    Trace *whichTrace,
                                    vector<Timeline *>& windows,
                                    vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  // TODO: non color mode is color mode !!
  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    windows[ windows.size() - 1 ]->setFunctionLineColorMode();
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    return true;
  else
    return false;

  return true;
}

void WindowNonColorMode::printLine( ofstream& cfgFile,
                                    const vector<Timeline *>::const_iterator it )
{
  // TODO: non color mode is color mode !!
  cfgFile << OLDCFG_TAG_WNDW_NON_COLOR_MODE << " " << ( ( *it )->isFunctionLineColorSet() ?
      OLDCFG_VAL_FALSE : OLDCFG_VAL_TRUE ) << endl;
}


string WindowColorMode::tagCFG = OLDCFG_TAG_WNDW_COLOR_MODE;

bool WindowColorMode::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strMode;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strMode, ' ' );

  if( windows[ windows.size() - 1 ]->isFunctionLineColorSet() )
    return true;

  if ( strMode.compare( OLDCFG_VAL_COLOR_MODE_GRADIENT ) == 0 )
    windows[ windows.size() - 1 ]->setGradientColorMode();
  else if ( strMode.compare( OLDCFG_VAL_COLOR_MODE_NULL_GRADIENT ) == 0 )
    windows[ windows.size() - 1 ]->setNotNullGradientColorMode();
  else if( strMode.compare( CFG_VAL_COLOR_MODE_PUNCTUAL ) == 0 )
    windows[ windows.size() - 1 ]->setPunctualColorMode();
  else if( strMode.compare( CFG_VAL_COLOR_MODE_FUSED_LINES ) == 0 )
    windows[ windows.size() - 1 ]->setFusedLinesColorMode();

  return true;
}

void WindowColorMode::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  if ( ( *it )->isGradientColorSet() || ( *it )->isNotNullGradientColorSet() )
  {
    cfgFile << OLDCFG_TAG_WNDW_COLOR_MODE << " " <<
    ( ( *it )->isGradientColorSet() ?
      OLDCFG_VAL_COLOR_MODE_GRADIENT : OLDCFG_VAL_COLOR_MODE_NULL_GRADIENT ) << endl;
  }
  else if ( ( *it )->isPunctualColorSet() )
    cfgFile << OLDCFG_TAG_WNDW_COLOR_MODE << " " << CFG_VAL_COLOR_MODE_PUNCTUAL <<endl;
  else if ( ( *it )->isFusedLinesColorSet() )
    cfgFile << OLDCFG_TAG_WNDW_COLOR_MODE << " " << CFG_VAL_COLOR_MODE_FUSED_LINES <<endl;
}

string WindowCustomColorEnabled::tagCFG = OLDCFG_TAG_WNDW_CUSTOM_COLOR_ENABLED;

bool WindowCustomColorEnabled::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    windows[ windows.size() - 1 ]->setUseCustomPalette( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    windows[ windows.size() - 1 ]->setUseCustomPalette( true );
  else
    return false;

  return true;
}

void WindowCustomColorEnabled::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_CUSTOM_COLOR_ENABLED << " " << ( ( *it )->getUseCustomPalette() ?
      OLDCFG_VAL_TRUE : OLDCFG_VAL_FALSE ) << endl;
}


string WindowCustomColorPalette::tagCFG = OLDCFG_TAG_WNDW_CUSTOM_COLOR_PALETTE;

bool WindowCustomColorPalette::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
 
  istringstream sstrColorItem;
  string strColorItem;
  istringstream sstrTmp;
  string strTmp;
  while ( !line.eof() ) 
  {
    TSemanticValue tmpValue;
    rgb tmpRGB;
    int tmpComponent;

    sstrColorItem.clear();
    getline( line, strColorItem, '}' ); // { value : r, g, b }
    if( line.eof() )
      break;
    sstrColorItem.str( strColorItem );
    
    sstrTmp.clear();
    getline( sstrColorItem, strTmp, '{' ); // get '{'
    getline( sstrColorItem, strTmp, ':' ); // value
    sstrTmp.str( strTmp );
    if ( !( sstrTmp >> tmpValue ) ) 
      return false;
  
    sstrTmp.clear();
    getline( sstrColorItem, strTmp, ',' ); // color red
    sstrTmp.str( strTmp );
    if ( !( sstrTmp >> tmpComponent ) ) 
      return false;
    tmpRGB.red = tmpComponent;

    sstrTmp.clear();
    getline( sstrColorItem, strTmp, ',' ); // color green
    sstrTmp.str( strTmp );
    if ( !( sstrTmp >> tmpComponent ) ) 
      return false;
    tmpRGB.green = tmpComponent;

    sstrTmp.clear();
    getline( sstrColorItem, strTmp, '}' ); // color blue
    sstrTmp.str( strTmp );
    if ( !( sstrTmp >> tmpComponent ) ) 
      return false;
    tmpRGB.blue = tmpComponent;

    windows[ windows.size() - 1 ]->getCodeColor().setCustomColor( tmpValue, tmpRGB );
  }

  return true;
}

void WindowCustomColorPalette::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  const CodeColor& tmpCodeColor = ( *it )->getCodeColor();

  if( tmpCodeColor.existCustomColors() )
  {
    cfgFile << OLDCFG_TAG_WNDW_CUSTOM_COLOR_PALETTE << " ";

    const std::map<TSemanticValue, rgb>& tmpPalette = tmpCodeColor.getCustomPalette();
    for( std::map<TSemanticValue, rgb>::const_iterator itMap = tmpPalette.begin(); itMap != tmpPalette.end(); ++itMap )
    {
      cfgFile << "{" << itMap->first << ":" << (int) itMap->second.red << "," << (int) itMap->second.green << "," << (int) itMap->second.blue << "}";
      if ( itMap != --tmpPalette.end() )
        cfgFile << ",";
    }
    
    cfgFile << endl;
  }
}


string WindowSemanticScaleMinAtZero::tagCFG = OLDCFG_TAG_WNDW_SEMANTIC_SCALE_MIN_AT_ZERO;

bool WindowSemanticScaleMinAtZero::parseLine( KernelConnection *whichKernel, istringstream& line,
                                              Trace *whichTrace,
                                              vector<Timeline *>& windows,
                                              vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    windows[ windows.size() - 1 ]->setSemanticScaleMinAtZero( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    windows[ windows.size() - 1 ]->setSemanticScaleMinAtZero( true );
  else
    return false;

  return true;
}

void WindowSemanticScaleMinAtZero::printLine( ofstream& cfgFile,
                                              const vector<Timeline *>::const_iterator it )
{
  cfgFile << WindowSemanticScaleMinAtZero::tagCFG << " " << ( ( *it )->getSemanticScaleMinAtZero() ?
      OLDCFG_VAL_TRUE : OLDCFG_VAL_FALSE ) << endl;
}


string WindowUnits::tagCFG = OLDCFG_TAG_WNDW_UNITS;

bool WindowUnits::parseLine( KernelConnection *whichKernel, istringstream& line,
                             Trace *whichTrace,
                             vector<Timeline *>& windows,
                             vector<Histogram *>& histograms )
{
  string strUnits;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strUnits, ' ' );

  if ( strUnits.compare( OLDCFG_VAL_TIMEUNIT_NS ) == 0 )
    windows[ windows.size() - 1 ]->setTimeUnit( NS );
  else if ( strUnits.compare( OLDCFG_VAL_TIMEUNIT_US ) == 0 )
    windows[ windows.size() - 1 ]->setTimeUnit( US );
  else if ( strUnits.compare( OLDCFG_VAL_TIMEUNIT_MS ) == 0 )
    windows[ windows.size() - 1 ]->setTimeUnit( MS );
  else if ( strUnits.compare( OLDCFG_VAL_TIMEUNIT_S ) == 0 )
    windows[ windows.size() - 1 ]->setTimeUnit( SEC );
  else if ( strUnits.compare( OLDCFG_VAL_TIMEUNIT_M ) == 0 )
    windows[ windows.size() - 1 ]->setTimeUnit( MIN );
  else if ( strUnits.compare( OLDCFG_VAL_TIMEUNIT_H ) == 0 )
    windows[ windows.size() - 1 ]->setTimeUnit( HOUR );
  else if ( strUnits.compare( OLDCFG_VAL_TIMEUNIT_D ) == 0 )
    windows[ windows.size() - 1 ]->setTimeUnit( DAY );

  return true;
}

void WindowUnits::printLine( ofstream& cfgFile,
                             const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_UNITS << " ";
  switch ( ( *it )->getTimeUnit() )
  {
    case NS:
      cfgFile << OLDCFG_VAL_TIMEUNIT_NS;
      break;
    case US:
      cfgFile << OLDCFG_VAL_TIMEUNIT_US;
      break;
    case MS:
      cfgFile << OLDCFG_VAL_TIMEUNIT_MS;
      break;
    case SEC:
      cfgFile << OLDCFG_VAL_TIMEUNIT_S;
      break;
    case MIN:
      cfgFile << OLDCFG_VAL_TIMEUNIT_M;
      break;
    case HOUR:
      cfgFile << OLDCFG_VAL_TIMEUNIT_H;
      break;
    case DAY:
      cfgFile << OLDCFG_VAL_TIMEUNIT_D;
      break;
  }

  cfgFile << endl;
}


string WindowOperation::tagCFG = OLDCFG_TAG_WNDW_OPERATION;

bool WindowOperation::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strFunction;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( !windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  getline( line, strFunction );

  return windows[ windows.size() - 1 ]->setLevelFunction( DERIVED, strFunction );
}

void WindowOperation::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_OPERATION << " " << ( *it )->getLevelFunction( DERIVED ) << endl;
}


string WindowMaximumY::tagCFG = OLDCFG_TAG_WNDW_MAXIMUM_Y;

bool WindowMaximumY::parseLine( KernelConnection *whichKernel, istringstream& line,
                                Trace *whichTrace,
                                vector<Timeline *>& windows,
                                vector<Histogram *>& histograms )
{
  string strMaximum;
  TSemanticValue maximum;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strMaximum, ' ' );
  istringstream tmpStream( strMaximum );
  if ( !( tmpStream >> maximum ) )
    return false;

  windows[ windows.size() - 1 ]->setMaximumY( maximum );

  return true;
}

void WindowMaximumY::printLine( ofstream& cfgFile,
                                const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_MAXIMUM_Y << " " << ( *it )->getMaximumY() << endl;
}


string WindowMinimumY::tagCFG = OLDCFG_TAG_WNDW_MINIMUM_Y;

bool WindowMinimumY::parseLine( KernelConnection *whichKernel, istringstream& line,
                                Trace *whichTrace,
                                vector<Timeline *>& windows,
                                vector<Histogram *>& histograms )
{
  string strMinimum;
  TSemanticValue minimum;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strMinimum, ' ' );
  istringstream tmpStream( strMinimum );
  if ( !( tmpStream >> minimum ) )
    return false;

  windows[ windows.size() - 1 ]->setMinimumY( minimum );

  return true;
}

void WindowMinimumY::printLine( ofstream& cfgFile,
                                const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_MINIMUM_Y << " " << ( *it )->getMinimumY() << endl;
}


string WindowComputeYMax::tagCFG = OLDCFG_TAG_WNDW_COMPUTE_Y_MAX;

bool WindowComputeYMax::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    windows[ windows.size() - 1 ]->setComputeYMaxOnInit( false );
  else if ( strBool == "" || strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    windows[ windows.size() - 1 ]->setComputeYMaxOnInit( true );
  else
    return false;

  return true;
}

void WindowComputeYMax::printLine( ofstream& cfgFile,
                                   const SaveOptions& options,
                                   const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_COMPUTE_Y_MAX << " ";
  if ( options.windowComputeYMaxOnLoad )
    cfgFile << OLDCFG_VAL_TRUE;
  else
    cfgFile << OLDCFG_VAL_FALSE;
  cfgFile << endl;
}


string WindowLevel::tagCFG = OLDCFG_TAG_WNDW_LEVEL;

bool WindowLevel::parseLine( KernelConnection *whichKernel, istringstream& line,
                             Trace *whichTrace,
                             vector<Timeline *>& windows,
                             vector<Histogram *>& histograms )
{
  string strLevel;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strLevel, ' ' );

  TTraceLevel tmpLevel = static_cast<TTraceLevel>( stringToLevel( strLevel ) );
  if ( tmpLevel >= TTraceLevel::SYSTEM && tmpLevel <= TTraceLevel::CPU && !whichTrace->existResourceInfo() )
    return false;

  windows[ windows.size() - 1 ]->setLevel( tmpLevel );

  return true;
}

void WindowLevel::printLine( ofstream& cfgFile,
                             const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_LEVEL << " " << levelToString( static_cast<TWindowLevel>( ( *it )->getLevel() ) ) << endl;
}


string WindowIdentifiers::tagCFG = OLDCFG_TAG_WNDW_IDENTIFIERS;

bool WindowIdentifiers::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string strID;
  PRV_UINT16 id;
  PRV_UINT16 numID = 0;
  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( !windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  while ( !line.eof() )
  {
    getline( line, strID, ' ' );
    istringstream tmpStream( strID );
    if ( !( tmpStream >> id ) )
      return false;

    if ( windows[ id - 1 ] == nullptr )
      return false;

    windows[ windows.size() - 1 ]->setParent( numID, windows[ id - 1 ] );
    numID++;
  }

  return true;
}

void WindowIdentifiers::printLine( ofstream& cfgFile,
                                   const vector<Timeline *>& allWindows,
                                   const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_IDENTIFIERS << " ";
  cfgFile << CFGLoader::findWindowBackwards( ( *it )->getParent( 0 ), allWindows, it ) + 1 << " ";
  cfgFile << CFGLoader::findWindowBackwards( ( *it )->getParent( 1 ), allWindows, it ) + 1;
  cfgFile << endl;
}


string WindowZoomObjects::tagCFG = OLDCFG_TAG_WNDW_ZOOM_OBJECTS;

bool WindowZoomObjects::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string strObject;
  TObjectOrder firstObject;
  TObjectOrder lastObject;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strObject, ' ' );
  istringstream tmpStreamFirst( strObject );
  if ( !( tmpStreamFirst >> firstObject ) )
    return false;

  if( firstObject > windows[ windows.size() - 1 ]->getWindowLevelObjects() - 1 )
    firstObject = windows[ windows.size() - 1 ]->getWindowLevelObjects() - 1;

  getline( line, strObject, ' ' );
  istringstream tmpStreamLast( strObject );
  if ( !( tmpStreamLast >> lastObject ) )
    return false;

  if( lastObject > windows[ windows.size() - 1 ]->getWindowLevelObjects() - 1 )
    lastObject = windows[ windows.size() - 1 ]->getWindowLevelObjects() - 1;

  windows[ windows.size() - 1 ]->addZoom( firstObject, lastObject );

  return true;
}

void WindowZoomObjects::printLine( ofstream& cfgFile,
                                   const vector<Timeline *>::const_iterator it )
{
  pair<TObjectOrder, TObjectOrder> currentZoom = ( *it )->getZoomSecondDimension();
  if( currentZoom.first > 0 || currentZoom.second < ( *it )->getWindowLevelObjects() - 1 )
  {
    cfgFile << OLDCFG_TAG_WNDW_ZOOM_OBJECTS << " ";
    cfgFile << currentZoom.first << " ";
    cfgFile << currentZoom.second;
    cfgFile << endl;
  }
}


string WindowScaleRelative::tagCFG = OLDCFG_TAG_WNDW_SCALE_RELATIVE;

bool WindowScaleRelative::parseLine( KernelConnection *whichKernel, istringstream& line,
                                     Trace *whichTrace,
                                     vector<Timeline *>& windows,
                                     vector<Histogram *>& histograms )
{
  string strEndTime;
  double percEndTime;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strEndTime );
  istringstream tmpStream( strEndTime );
  if ( !( tmpStream >> percEndTime ) )
    return false;

  windows[ windows.size() - 1 ]->setWindowEndTime(
    ( windows[ windows.size() - 1 ]->getTrace() )->getEndTime() * percEndTime );

  return true;
}

void WindowScaleRelative::printLine( ofstream& cfgFile,
                                     const SaveOptions& options,
                                     const vector<Timeline *>::const_iterator it )
{
  if ( options.windowScaleRelative )
  {
    cfgFile << OLDCFG_TAG_WNDW_SCALE_RELATIVE << " ";
    double maxScale = ( *it )->getTrace()->getEndTime() / ( double )( *it )->getWidth();
    cfgFile << ( ( *it )->getWindowEndTime() / ( *it )->getWidth() ) / maxScale;
    cfgFile << endl;
  }
}


string WindowEndTimeRelative::tagCFG = CFG_TAG_WNDW_END_TIME_RELATIVE;

bool WindowEndTimeRelative::parseLine( KernelConnection *whichKernel, istringstream& line,
                                       Trace *whichTrace,
                                       vector<Timeline *>& windows,
                                       vector<Histogram *>& histograms )
{
  string strEndTime;
  double percEndTime;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strEndTime );
  istringstream tmpStream( strEndTime );
  if ( !( tmpStream >> percEndTime ) )
    return false;

  windows[ windows.size() - 1 ]->setWindowEndTime(
    ( windows[ windows.size() - 1 ]->getTrace() )->getEndTime() * percEndTime );

  return true;
}

void WindowEndTimeRelative::printLine( ofstream& cfgFile,
                                       const SaveOptions& options,
                                       const vector<Timeline *>::const_iterator it )
{
  if ( options.windowScaleRelative )
  {
    cfgFile << CFG_TAG_WNDW_END_TIME_RELATIVE << " ";
    cfgFile << ( *it )->getWindowEndTime() / ( *it )->getTrace()->getEndTime();
    cfgFile << endl;
  }
}

bool genericParseObjects( istringstream& line,
                          TObjectOrder numObjects,
                          TObjectOrder beginObject,
                          vector<bool>& onVector,
                          bool numbers = false )
{
  string strValue;
  bool value;

  for ( TObjectOrder iObj = 0; iObj < numObjects; ++iObj )
  {
    if ( iObj == numObjects - 1 )
    {
      getline( line, strValue, '}' );
      strValue.erase( strValue.length() - 1 );
      strValue.erase( 0, 1 );
    }
    else
    {
      getline( line, strValue, ',' );
      strValue.erase( 0, 1 );
    }

    if ( numbers )
    {
      istringstream tmpValue( strValue );
      if ( !( tmpValue >> value ) )
        return false;
    }
    else
    {
      value = true;
      if ( strValue == "None" )
        value = false;
    }

    onVector[ beginObject + iObj ] = value;
  }

  return true;
}


string WindowObject::tagCFG = OLDCFG_TAG_WNDW_OBJECT;

bool WindowObject::parseLine( KernelConnection *whichKernel, istringstream& line, Trace *whichTrace,
                              vector<Timeline *>& windows,
                              vector<Histogram *>& histograms )
{
  string strVoid;
  string strLevel;
  TTraceLevel level;
  string strNumObjects;
  TObjectOrder numObjects;
  vector<bool> selObjects;
  istringstream tmpNumObjects;
  Timeline *win = windows[ windows.size() - 1 ];

  if ( win == nullptr )
    return false;

  getline( line, strLevel, ' ' );
  level = static_cast<TTraceLevel>( stringToLevel( strLevel ) );

  getline( line, strVoid, ' ' );

  switch ( level )
  {
    case TTraceLevel::APPLICATION:
      {
        win->getSelectedRows( TTraceLevel::APPLICATION, selObjects );
        getline( line, strNumObjects, ',' );
        tmpNumObjects.str( strNumObjects );
        if ( !( tmpNumObjects >> numObjects ) )
          return false;
        if ( numObjects > whichTrace->totalApplications() )
          return true;
        getline( line, strVoid, '{' );
        if ( !genericParseObjects( line, numObjects, 0, selObjects, win->getLevel() == level ) )
          return false;
        win->setSelectedRows( TTraceLevel::APPLICATION, selObjects );
      }
      break;

    case TTraceLevel::TASK:
      {
        string strAppl;
        TApplOrder appl;
        win->getSelectedRows( TTraceLevel::TASK, selObjects );
        getline( line, strAppl, ',' );
        istringstream tmpAppl( strAppl );
        if ( !( tmpAppl >> appl ) )
          return false;
        if ( appl >= whichTrace->totalApplications() )
          return true;

        getline( line, strNumObjects, ',' );
        tmpNumObjects.str( strNumObjects );
        if ( !( tmpNumObjects >> numObjects ) )
          return false;
        TApplOrder tmpNumAppl;
        TTaskOrder tmpNumTask;
        whichTrace->getTaskLocation( whichTrace->getLastTask( appl ), tmpNumAppl, tmpNumTask );
        if ( numObjects > tmpNumTask + 1 )
          return true;

        getline( line, strVoid, '{' );
        TObjectOrder beginObject = win->getTrace()->getGlobalTask( appl, 0 );
        if ( !genericParseObjects( line, numObjects, beginObject, selObjects, win->getLevel() == level ) )
          return false;
        win->setSelectedRows( TTraceLevel::TASK, selObjects );
      }
      break;

    case TTraceLevel::THREAD:
      {
        string strAppl, strTask;
        TApplOrder appl, task;
        win->getSelectedRows( TTraceLevel::THREAD, selObjects );
        getline( line, strAppl, ',' );
        istringstream tmpAppl( strAppl );
        if ( !( tmpAppl >> appl ) )
          return false;
        if ( appl >= whichTrace->totalApplications() )
          return true;

        getline( line, strTask, ',' );
        istringstream tmpTask( strTask );
        if ( !( tmpTask >> task ) )
          return false;
        TApplOrder tmpNumAppl;
        TTaskOrder tmpNumTask;
        whichTrace->getTaskLocation( whichTrace->getLastTask( appl ), tmpNumAppl, tmpNumTask );
        if ( task > tmpNumTask )
          return true;

        getline( line, strNumObjects, ',' );
        tmpNumObjects.str( strNumObjects );
        if ( !( tmpNumObjects >> numObjects ) )
          return false;
        TThreadOrder tmpNumThread;
        whichTrace->getThreadLocation( whichTrace->getLastThread( appl, task ), tmpNumAppl, tmpNumTask, tmpNumThread );
        if ( numObjects > tmpNumThread + 1 )
          return true;

        getline( line, strVoid, '{' );
        TObjectOrder beginObject = win->getTrace()->getGlobalThread( appl, task, 0 );
        if ( !genericParseObjects( line, numObjects, beginObject, selObjects, win->getLevel() == level ) )
          return false;
        win->setSelectedRows( TTraceLevel::THREAD, selObjects );
      }
      break;

    case TTraceLevel::NODE:
      {
        win->getSelectedRows( TTraceLevel::NODE, selObjects );
        getline( line, strNumObjects, ',' );
        tmpNumObjects.str( strNumObjects );
        if ( !( tmpNumObjects >> numObjects ) )
          return false;
        if ( numObjects > whichTrace->totalNodes() )
          return true;

        getline( line, strVoid, '{' );
        if ( !genericParseObjects( line, numObjects, 0, selObjects, win->getLevel() == level ) )
          return false;
        win->setSelectedRows( TTraceLevel::NODE, selObjects );
      }
      break;

    case TTraceLevel::CPU:
      {
        string strNode;
        TApplOrder node;
        win->getSelectedRows( TTraceLevel::CPU, selObjects );
        getline( line, strNode, ',' );
        istringstream tmpNode( strNode );
        if ( !( tmpNode >> node ) )
          return false;
        if ( node >= whichTrace->totalNodes() )
          return true;

        getline( line, strNumObjects, ',' );
        tmpNumObjects.str( strNumObjects );
        if ( !( tmpNumObjects >> numObjects ) )
          return false;
        TApplOrder tmpNumNode;
        TTaskOrder tmpNumCPU;
        whichTrace->getCPULocation( whichTrace->getLastCPU( node ), tmpNumNode, tmpNumCPU );
        if ( numObjects > tmpNumCPU + 1 )
          return true;

        getline( line, strVoid, '{' );
        TObjectOrder beginObject = win->getTrace()->getGlobalCPU( node, 1 );
        if ( !genericParseObjects( line, numObjects, beginObject, selObjects, win->getLevel() == level ) )
          return false;
        win->setSelectedRows( TTraceLevel::CPU, selObjects );
      }
      break;

    default:
      break;
  }

  return true;
}

void genericWriteObjects( ofstream& cfgFile, vector<bool>& selected, bool numbers )
{
  for ( vector<bool>::iterator it = selected.begin(); it != selected.end(); ++it )
  {
    if ( *it )
    {
      if ( numbers )
        cfgFile << "1";
      else
        cfgFile << "All";
    }
    else
    {
      if ( numbers )
        cfgFile << "0";
      else
        cfgFile << "None";
    }
    if ( it != --selected.end() )
      cfgFile << ", ";
  }
}

void writeAppl( ofstream& cfgFile,
                const vector<Timeline *>::const_iterator it )
{
  vector<bool> selectedSet;

  ( *it )->getSelectedRows( TTraceLevel::APPLICATION, selectedSet );
  cfgFile << OLDCFG_TAG_WNDW_OBJECT << " appl { " << selectedSet.size() << ", { ";
  genericWriteObjects( cfgFile, selectedSet, ( *it )->getLevel() == TTraceLevel::APPLICATION );
  cfgFile << " } }" << endl;
}

void writeTask( ofstream& cfgFile,
                const vector<Timeline *>::const_iterator it,
                TApplOrder whichAppl )
{
  vector<bool> selectedSet;

  ( *it )->getSelectedRows( TTraceLevel::TASK, selectedSet,
                            ( *it )->getTrace()->getFirstTask( whichAppl ),
                            ( *it )->getTrace()->getLastTask( whichAppl ) );
  cfgFile << OLDCFG_TAG_WNDW_OBJECT << " task { " << whichAppl << ", " << selectedSet.size() << ", { ";
  genericWriteObjects( cfgFile, selectedSet, ( *it )->getLevel() == TTraceLevel::TASK );
  cfgFile << " } }" << endl;
}

void writeTasks( ofstream& cfgFile,
                 const vector<Timeline *>::const_iterator it )
{
  vector<TTaskOrder> tmpSel;
  vector<TThreadOrder> tmpSelThreads;
  vector<bool> selectedAppl;

  ( *it )->getSelectedRows( TTraceLevel::APPLICATION, selectedAppl );
  for ( TApplOrder iAppl = 0; iAppl < ( *it )->getTrace()->totalApplications(); ++iAppl )
  {
    if ( !selectedAppl[ iAppl ] )
      continue;
    TTaskOrder begin = ( *it )->getTrace()->getFirstTask( iAppl );
    TTaskOrder last = ( *it )->getTrace()->getLastTask( iAppl );
    ( *it )->getSelectedRows( TTraceLevel::TASK, tmpSel, begin, last );
    TApplOrder tmpAppl;
    TTaskOrder beginTask;
    TTaskOrder lastTask;
    ( *it )->getTrace()->getTaskLocation( begin, tmpAppl, beginTask );
    ( *it )->getTrace()->getTaskLocation( last, tmpAppl, lastTask );
    TThreadOrder beginThread = ( *it )->getTrace()->getFirstThread( iAppl, beginTask );
    TThreadOrder lastThread = ( *it )->getTrace()->getLastThread( iAppl, lastTask );
    ( *it )->getSelectedRows( TTraceLevel::THREAD, tmpSelThreads, beginThread, lastThread );
    if ( ( /*tmpSel.size() > 0  &&*/ tmpSel.size() != ( TObjectOrder )( last - begin + 1 ) )
         || tmpSelThreads.size() != ( TObjectOrder )( lastThread - beginThread + 1 )
       )
      writeTask( cfgFile, it, iAppl );
  }
}

void writeThread( ofstream& cfgFile,
                  const vector<Timeline *>::const_iterator it,
                  TApplOrder whichAppl,
                  TTaskOrder whichTask )
{
  vector<bool> selectedSet;

  ( *it )->getSelectedRows( TTraceLevel::THREAD, selectedSet,
                            ( *it )->getTrace()->getFirstThread( whichAppl, whichTask ),
                            ( *it )->getTrace()->getLastThread( whichAppl, whichTask ) );
  cfgFile << OLDCFG_TAG_WNDW_OBJECT << " thread { " << whichAppl << ", ";
  cfgFile << whichTask << ", " << selectedSet.size() << ", { ";
  genericWriteObjects( cfgFile, selectedSet, ( *it )->getLevel() == TTraceLevel::THREAD );
  cfgFile << " } }" << endl;
}

void writeThreads( ofstream& cfgFile,
                   const vector<Timeline *>::const_iterator it )
{
  vector<TObjectOrder> tmpSel;
  vector<bool> selectedAppl;
  vector<bool> selectedTask;

  ( *it )->getSelectedRows( TTraceLevel::APPLICATION, selectedAppl );
  for ( TApplOrder iAppl = 0; iAppl < ( *it )->getTrace()->totalApplications(); ++iAppl )
  {
    if ( !selectedAppl[ iAppl ] )
      continue;
    TTaskOrder beginTask = ( *it )->getTrace()->getFirstTask( iAppl );
    TTaskOrder lastTask = ( *it )->getTrace()->getLastTask( iAppl );
    ( *it )->getSelectedRows( TTraceLevel::TASK, selectedTask, beginTask, lastTask );
    for ( TTaskOrder iTask = beginTask; iTask <= lastTask; ++iTask )
    {
      if ( !selectedTask[ iTask - beginTask ] )
        continue;
      TTaskOrder begin = ( *it )->getTrace()->getFirstThread( iAppl, iTask - beginTask );
      TTaskOrder last = ( *it )->getTrace()->getLastThread( iAppl, iTask - beginTask );
      ( *it )->getSelectedRows( TTraceLevel::THREAD, tmpSel, begin, last );
      if ( /*tmpSel.size() > 0  &&*/ tmpSel.size() != ( TObjectOrder )( last - begin + 1 ) )
        writeThread( cfgFile, it, iAppl, iTask - beginTask );
    }
  }
}

void writeNode( ofstream& cfgFile,
                const vector<Timeline *>::const_iterator it )
{
  vector<bool> selectedSet;

  ( *it )->getSelectedRows( TTraceLevel::NODE, selectedSet );
  cfgFile << OLDCFG_TAG_WNDW_OBJECT << " node { " << selectedSet.size() << ", { ";
  genericWriteObjects( cfgFile, selectedSet, ( *it )->getLevel() == TTraceLevel::NODE );
  cfgFile << " } }" << endl;
}

void writeCPU( ofstream& cfgFile,
               const vector<Timeline *>::const_iterator it,
               TNodeOrder whichNode )
{
  vector<bool> selectedSet;

  ( *it )->getSelectedRows( TTraceLevel::CPU, selectedSet,
                            ( *it )->getTrace()->getFirstCPU( whichNode ) - 1,
                            ( *it )->getTrace()->getLastCPU( whichNode ) - 1 );
  cfgFile << OLDCFG_TAG_WNDW_OBJECT << " cpu { " << whichNode << ", " << selectedSet.size() << ", { ";
  genericWriteObjects( cfgFile, selectedSet, ( *it )->getLevel() == TTraceLevel::CPU );
  cfgFile << " } }" << endl;
}

void writeCPUs( ofstream& cfgFile,
                const vector<Timeline *>::const_iterator it )
{
  vector<TObjectOrder> tmpSel;
  vector<TObjectOrder> tmpSelCPU;
  vector<bool> selectedNode;

  ( *it )->getSelectedRows( TTraceLevel::NODE, selectedNode );
  for ( TNodeOrder iNode = 0; iNode < ( *it )->getTrace()->totalNodes(); ++iNode )
  {
    if ( !selectedNode[ iNode ] )
      continue;
    TCPUOrder begin = ( *it )->getTrace()->getFirstCPU( iNode );
    TCPUOrder last = ( *it )->getTrace()->getLastCPU( iNode );
    ( *it )->getSelectedRows( TTraceLevel::CPU, tmpSel, begin, last );
    if ( tmpSel.size() > 0  && tmpSel.size() != ( TObjectOrder )( last - begin + 1 ) )
      writeCPU( cfgFile, it, iNode );
  }
}

void WindowObject::printLine( ofstream& cfgFile,
                              const vector<Timeline *>::const_iterator it )
{
  vector<TObjectOrder> selected;

  switch ( ( *it )->getLevel() )
  {
    case TTraceLevel::WORKLOAD:
    case TTraceLevel::APPLICATION:
      writeAppl( cfgFile, it );
      break;

    case TTraceLevel::TASK:
      writeAppl( cfgFile, it );
      writeTasks( cfgFile, it );
      break;

    case TTraceLevel::THREAD:
      writeAppl( cfgFile, it );
      writeTasks( cfgFile, it );
      writeThreads( cfgFile, it );
      break;

    case TTraceLevel::SYSTEM:
    case TTraceLevel::NODE:
      writeNode( cfgFile, it );
      break;

    case TTraceLevel::CPU:
      writeNode( cfgFile, it );
      writeCPUs( cfgFile, it );
      break;

    default:
      break;
  }
}


string WindowBeginTime::tagCFG = OLDCFG_TAG_WNDW_BEGIN_TIME;

bool WindowBeginTime::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strTime;
  TRecordTime auxTime;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strTime, ' ' );
  istringstream tmpTime( strTime );

  if ( !( tmpTime >> auxTime ) )
    return false;

  if ( auxTime < whichTrace->getEndTime() )
    windows[ windows.size() - 1 ]->setWindowBeginTime( auxTime );
  else
    windows[ windows.size() - 1 ]->setWindowBeginTime( 0.0 );

  return true;
}

void WindowBeginTime::printLine( ofstream& cfgFile,
                                 const SaveOptions& options,
                                 const vector<Timeline *>::const_iterator it )
{
  if ( !options.windowBeginTimeRelative )
  {
    cfgFile << OLDCFG_TAG_WNDW_BEGIN_TIME << " " << ( *it )->getWindowBeginTime() << endl;
  }
}


string WindowEndTime::tagCFG = "";

bool WindowEndTime::parseLine( KernelConnection *whichKernel, istringstream& line,
                               Trace *whichTrace,
                               vector<Timeline *>& windows,
                               vector<Histogram *>& histograms )
{
  string strTime;
  TRecordTime auxTime;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strTime, ' ' );
  istringstream tmpTime( strTime );

  if ( !( tmpTime >> auxTime ) )
    return false;

  // endTime = auxTime;

  return true;
}

void WindowEndTime::printLine( ofstream& cfgFile,
                               const SaveOptions& options,
                               const vector<Timeline *>::const_iterator it )
{
}


string WindowStopTime::tagCFG = OLDCFG_TAG_WNDW_STOP_TIME;

bool WindowStopTime::parseLine( KernelConnection *whichKernel, istringstream& line,
                                Trace *whichTrace,
                                vector<Timeline *>& windows,
                                vector<Histogram *>& histograms )
{
  string strTime;
  TRecordTime auxTime;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strTime, ' ' );
  istringstream tmpTime( strTime );

  if ( !( tmpTime >> auxTime ) )
    return false;

  if ( auxTime <= whichTrace->getEndTime() )
    windows[ windows.size() - 1 ]->setWindowEndTime( auxTime );
  else
    windows[ windows.size() - 1 ]->setWindowEndTime( whichTrace->getEndTime() );
  return true;
}

void WindowStopTime::printLine( ofstream& cfgFile,
                                const SaveOptions& options,
                                const vector<Timeline *>::const_iterator it )
{
  if ( !options.windowScaleRelative )
  {
    cfgFile << OLDCFG_TAG_WNDW_STOP_TIME << " " << ( *it )->getWindowEndTime() << endl;
 }
}


string WindowBeginTimeRelative::tagCFG = OLDCFG_TAG_WNDW_BEGIN_TIME_RELATIVE;

bool WindowBeginTimeRelative::parseLine( KernelConnection *whichKernel,
                                         istringstream& line,
                                         Trace *whichTrace,
                                         vector<Timeline *>& windows,
                                         vector<Histogram *>& histograms )
{
  string strPercentage;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strPercentage, ' ' );
  istringstream tmpStream( strPercentage );
  TSemanticValue percentage;

  if ( !( tmpStream >> percentage ) )
    return false;

  windows[ windows.size() - 1 ]->setWindowBeginTime(
    ( windows[ windows.size() - 1 ]->getTrace() )->getEndTime() * percentage );

  return true;
}

void WindowBeginTimeRelative::printLine( ofstream& cfgFile,
    const SaveOptions& options,
    const vector<Timeline *>::const_iterator it )
{
  if ( options.windowBeginTimeRelative )
  {
    cfgFile << OLDCFG_TAG_WNDW_BEGIN_TIME_RELATIVE << " ";
    cfgFile << ( *it )->getWindowBeginTime() / ( *it )->getTrace()->getEndTime();
    cfgFile << endl;
  }
}


string WindowNumberOfRow::tagCFG = "";

bool WindowNumberOfRow::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  // DEPRECATED
  return true;
}

void WindowNumberOfRow::printLine( ofstream& cfgFile,
                                   const vector<Timeline *>::const_iterator it )
{
  // DEPRECATED
}


string WindowSelectedFunctions::tagCFG = OLDCFG_TAG_WNDW_SELECTED_FUNCTIONS;

bool WindowSelectedFunctions::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  PRV_UINT16 numFunctions;
  string strLevel;
  TWindowLevel level;
  string strFunction;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( !parseSelectedFunctionsNumFunctions( line, numFunctions ) )
    return false;

  for ( PRV_UINT16 i = 0; i < numFunctions; i++ )
  {
    parseSelectedFunctionsLevelAndFunction( line, strLevel, level, strFunction );

    // It's a semantic function
    if ( level != NONE )
    {
      bool result = windows[ windows.size() - 1 ]->setLevelFunction( level, strFunction );
      if ( !result )
        return false;
    }
    // It's a filter function
    else
    {
      if( windows[ windows.size() - 1 ]->isDerivedWindow() )
        return false;

      Filter *filter = windows[ windows.size() - 1 ]->getFilter();

      if ( strLevel.compare( OLDCFG_VAL_FILTER_OBJ_FROM ) == 0 )
        filter->setCommFromFunction( strFunction );
      else if ( strLevel.compare( OLDCFG_VAL_FILTER_OBJ_TO ) == 0 )
        filter->setCommToFunction( strFunction );
      else if ( strLevel.compare( OLDCFG_VAL_FILTER_COM_TAG ) == 0 )
        filter->setCommTagFunction( strFunction );
      else if ( strLevel.compare( OLDCFG_VAL_FILTER_COM_SIZE ) == 0 )
        filter->setCommSizeFunction( strFunction );
      else if ( strLevel.compare( OLDCFG_VAL_FILTER_COM_BW ) == 0 )
        filter->setBandWidthFunction( strFunction );
      else if ( strLevel.compare( OLDCFG_VAL_FILTER_EVT_TYPE ) == 0 )
      {
        filter->setEventTypeFunction( strFunction );
        if ( strFunction == "All" || strFunction == "None" )
          someEventsExist = true;
      }
      else if ( strLevel.compare( OLDCFG_VAL_FILTER_EVT_VALUE ) == 0 )
        filter->setEventValueFunction( strFunction );
    }
  }

  return true;
}

void WindowSelectedFunctions::printLine( ofstream& cfgFile,
    const vector<Timeline *>::const_iterator it )
{
  Filter *filter = nullptr;
  if( !( *it )->isDerivedWindow() )
    filter = ( *it )->getFilter();

  if( !( *it )->isDerivedWindow() )
    cfgFile << OLDCFG_TAG_WNDW_SELECTED_FUNCTIONS << " { 14, { ";
  else
    cfgFile << OLDCFG_TAG_WNDW_SELECTED_FUNCTIONS << " { 5, { ";

  if( !( *it )->isDerivedWindow() )
    cfgFile << "{" << OLDCFG_LVL_CPU << ", " << ( *it )->getLevelFunction( CPU ) << "}, ";
  cfgFile << "{" << OLDCFG_LVL_APPL << ", " << ( *it )->getLevelFunction( APPLICATION ) << "}, ";
  cfgFile << "{" << OLDCFG_LVL_TASK << ", " << ( *it )->getLevelFunction( TASK ) << "}, ";
  if( !( *it )->isDerivedWindow() )
    cfgFile << "{" << OLDCFG_LVL_THREAD << ", " << ( *it )->getLevelFunction( THREAD ) << "}, ";
  cfgFile << "{" << OLDCFG_LVL_NODE << ", " << ( *it )->getLevelFunction( NODE ) << "}, ";
  cfgFile << "{" << OLDCFG_LVL_SYSTEM << ", " << ( *it )->getLevelFunction( SYSTEM ) << "}, ";
  cfgFile << "{" << OLDCFG_LVL_WORKLOAD << ", " << ( *it )->getLevelFunction( WORKLOAD ) << "}, ";
  if( !( *it )->isDerivedWindow() )
  {
    cfgFile << "{" << OLDCFG_VAL_FILTER_OBJ_FROM << ", " << filter->getCommFromFunction() << "}, ";
    cfgFile << "{" << OLDCFG_VAL_FILTER_OBJ_TO << ", " << filter->getCommToFunction() << "}, ";
    cfgFile << "{" << OLDCFG_VAL_FILTER_COM_TAG << ", " << filter->getCommTagFunction() << "}, ";
    cfgFile << "{" << OLDCFG_VAL_FILTER_COM_SIZE << ", " << filter->getCommSizeFunction() << "}, ";
    cfgFile << "{" << OLDCFG_VAL_FILTER_COM_BW << ", " << filter->getBandWidthFunction() << "}, ";
    cfgFile << "{" << OLDCFG_VAL_FILTER_EVT_TYPE << ", " << filter->getEventTypeFunction() << "}, ";
    cfgFile << "{" << OLDCFG_VAL_FILTER_EVT_VALUE << ", " << filter->getEventValueFunction() << "} ";
  }
  cfgFile << "} }" << endl;
}


string WindowComposeFunctions::tagCFG = OLDCFG_TAG_WNDW_COMPOSE_FUNCTIONS;

bool WindowComposeFunctions::parseLine( KernelConnection *whichKernel,
                                        istringstream& line,
                                        Trace *whichTrace,
                                        vector<Timeline *>& windows,
                                        vector<Histogram *>& histograms )
{
  string tmpString;
  string strNumFunctions;
  PRV_UINT16 numFunctions;
  string strLevel;
  TWindowLevel level;
  string strFunction;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, tmpString, ' ' );
  getline( line, strNumFunctions, ',' );
  istringstream tmpNumFunctions( strNumFunctions );

  if ( !( tmpNumFunctions >> numFunctions ) )
    return false;

  getline( line, tmpString, '{' );
  for ( PRV_UINT16 i = 0; i < numFunctions; i++ )
  {
    getline( line, tmpString, '{' );
    getline( line, strLevel, ',' );
    getline( line, tmpString, ' ' );
    getline( line, strFunction, '}' );
    level = stringToLevel( strLevel );

    if ( level == EXTRATOPCOMPOSE1 )
    {
      windows[ windows.size() - 1 ]->addExtraCompose( TOPCOMPOSE1 );
      bool result = windows[ windows.size() - 1 ]->setExtraLevelFunction( TOPCOMPOSE1, numComposesExtraComposesInWindow, strFunction );
      numComposesExtraComposesInWindow++; // after return?
      if ( !result )
      {
        return false;
      }
    }
    else if ( level != NONE ) // It's a semantic function
    {
      // Compatibility code with old CFG versions
      switch ( level )
      {
        case WORKLOAD:
          level = COMPOSEWORKLOAD;
          break;
        case APPLICATION:
          level = COMPOSEAPPLICATION;
          break;
        case TASK:
          level = COMPOSETASK;
          break;
        case THREAD:
          level = COMPOSETHREAD;
          break;
        case SYSTEM:
          level = COMPOSESYSTEM;
          break;
        case NODE:
          level = COMPOSENODE;
          break;
        case CPU:
          level = COMPOSECPU;
          break;
        default:
          break;
      }

      bool result = windows[ windows.size() - 1 ]->setLevelFunction( level, strFunction );
      if ( !result )
      {
        return false;
      }
    }
    else
    {
      return false;
    }
  }

  return true;
}

void WindowComposeFunctions::printLine( ofstream& cfgFile,
                                        const vector<Timeline *>::const_iterator it )
{
  PRV_UINT16 numComposes = 9 + ( *it )->getExtraNumPositions( TOPCOMPOSE1 );

  cfgFile << OLDCFG_TAG_WNDW_COMPOSE_FUNCTIONS << " { " << numComposes << ", { ";

  cfgFile << "{" << OLDCFG_LVL_COMPOSE_CPU << ", ";
  cfgFile << ( *it )->getLevelFunction( COMPOSECPU ) << "}, ";

  cfgFile << "{" << OLDCFG_LVL_COMPOSE_APPL << ", ";
  cfgFile << ( *it )->getLevelFunction( COMPOSEAPPLICATION ) << "}, ";

  cfgFile << "{" << OLDCFG_LVL_COMPOSE_TASK << ", ";
  cfgFile << ( *it )->getLevelFunction( COMPOSETASK ) << "}, ";

  cfgFile << "{" << OLDCFG_LVL_COMPOSE_THREAD << ", ";
  cfgFile << ( *it )->getLevelFunction( COMPOSETHREAD ) << "}, ";

  cfgFile << "{" << OLDCFG_LVL_COMPOSE_NODE << ", ";
  cfgFile << ( *it )->getLevelFunction( COMPOSENODE ) << "}, ";

  cfgFile << "{" << OLDCFG_LVL_COMPOSE_SYSTEM << ", ";
  cfgFile << ( *it )->getLevelFunction( COMPOSESYSTEM ) << "}, ";

  cfgFile << "{" << OLDCFG_LVL_COMPOSE_WORKLOAD << ", ";
  cfgFile << ( *it )->getLevelFunction( COMPOSEWORKLOAD ) << "}, ";

  cfgFile << "{" << OLDCFG_LVL_TOPCOMPOSE1 << ", " << ( *it )->getLevelFunction( TOPCOMPOSE1 ) << "}, ";
  cfgFile << "{" << OLDCFG_LVL_TOPCOMPOSE2 << ", " << ( *it )->getLevelFunction( TOPCOMPOSE2 ) << "}";

  if ( ( *it )->getExtraNumPositions( TOPCOMPOSE1 ) == 0 )
  {
    cfgFile << " ";
  }
  else
  {
    cfgFile << ", ";
    for( size_t pos = 0; pos < ( *it )->getExtraNumPositions( TOPCOMPOSE1 ); ++pos )
    {
      cfgFile << "{" << CFG_LVL_EXTRATOPCOMPOSE1 << ", " << ( *it )->getExtraLevelFunction( TOPCOMPOSE1, pos ) << "}";
      if ( pos == ( *it )->getExtraNumPositions( TOPCOMPOSE1 ) - 1 )
        cfgFile << " ";
      else
        cfgFile << ", ";
    }
  }

  cfgFile << "} }" << endl;
}

string WindowSemanticModule::tagCFG = OLDCFG_TAG_WNDW_SEMANTIC_MODULE;

bool WindowSemanticModule::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strLevel;
  TWindowLevel level;
  string strFunction;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strLevel, ' ' );
  level = stringToLevel( strLevel );
  if ( level == NONE )
    return false;

  getline( line, strFunction, '{' );
  strFunction.erase( strFunction.length() - 1 ); // Final space.

  if ( ( level != EXTRATOPCOMPOSE1 && windows[ windows.size() - 1 ]->getLevelFunction( level ) == strFunction )
      ||
       ( level == EXTRATOPCOMPOSE1 &&
         windows[ windows.size() - 1 ]->getExtraLevelFunction( TOPCOMPOSE1, numSemanticParamExtraComposesInWindow ) == strFunction ) )
  {
    string tmpString;
    string strNumParam;
    TParamIndex numParam;

    getline( line, tmpString, ' ' );
    getline( line, strNumParam, ',' );
    istringstream tmpNumParam( strNumParam );
    if ( !( tmpNumParam >> numParam ) )
      return false;

    getline( line, tmpString, '{' );

    for ( TParamIndex i = 0; i < numParam; i++ )
    {
      string strNumValues;
      PRV_UINT32 numValues;
      TParamValue values;

      getline( line, tmpString, ' ' );

      getline( line, strNumValues, ' ' );
      istringstream tmpNumValues( strNumValues );
      if ( !( tmpNumValues >> numValues ) )
        return false;

      for ( PRV_UINT32 j = 0; j < numValues; j++ )
      {
        string strParamValue;
        double paramValue;

        if ( j < numValues - 1 )
          getline( line, strParamValue, ' ' );
        else if ( j == numValues - 1 )
        {
          if ( i < numParam - 1 )
            getline( line, strParamValue, ',' );
          else if ( i == numParam - 1 )
            getline( line, strParamValue, ' ' );
        }
        istringstream tmpParamValue( strParamValue );
        if ( !( tmpParamValue >> paramValue ) )
          return false;
        values.push_back( paramValue );
      }

      if ( level == EXTRATOPCOMPOSE1 )
      {
        windows[ windows.size() - 1 ]->setExtraFunctionParam( TOPCOMPOSE1, numSemanticParamExtraComposesInWindow, i, values );
        numSemanticParamExtraComposesInWindow++;
      }
      else
        windows[ windows.size() - 1 ]->setFunctionParam( level, i, values );
    }

  }

  return true;
}

void WindowSemanticModule::printLine( ofstream& cfgFile,
                                      const vector<Timeline *>::const_iterator it )
{
  for ( size_t position = 0; position < ( *it )->getExtraNumPositions( TOPCOMPOSE1 ); ++position )
  {
    TWindowLevel topLevel = TOPCOMPOSE1;
    for ( TParamIndex parIdx = 0; parIdx < ( *it )->getExtraFunctionNumParam( topLevel, position ); ++parIdx )
    {
      if ( parIdx == 0 )
      {
        cfgFile << OLDCFG_TAG_WNDW_SEMANTIC_MODULE << " " << CFG_LVL_EXTRATOPCOMPOSE1;
        cfgFile << " " << ( *it )->getExtraLevelFunction( topLevel, position ) << " { ";
        cfgFile << ( *it )->getExtraFunctionNumParam( topLevel, position ) << ", ";
        cfgFile << "{ ";
      }
      vector<double> v = ( *it )->getExtraFunctionParam( topLevel, position, parIdx );
      cfgFile << v.size();
      for ( vector<double>::iterator itVec = v.begin(); itVec != v.end(); ++itVec )
        cfgFile << " " << ( *itVec );
      if ( parIdx < ( *it )->getExtraFunctionNumParam( topLevel, position ) - 1 )
        cfgFile << ", ";
      else
        cfgFile << " } }" << endl;
    }
  }

  for ( int levelIdx = TOPCOMPOSE1; levelIdx <= TOPCOMPOSE2; ++levelIdx )
  {
    TWindowLevel topLevel = ( TWindowLevel ) levelIdx;
    for ( TParamIndex parIdx = 0; parIdx < ( *it )->getFunctionNumParam( topLevel ); ++parIdx )
    {
      if ( parIdx == 0 )
      {
        cfgFile << OLDCFG_TAG_WNDW_SEMANTIC_MODULE << " " << levelToString( topLevel );
        cfgFile << " " << ( *it )->getLevelFunction( topLevel ) << " { ";
        cfgFile << ( *it )->getFunctionNumParam( topLevel ) << ", ";
        cfgFile << "{ ";
      }
      vector<double> v = ( *it )->getFunctionParam( topLevel, parIdx );
      cfgFile << v.size();
      for ( vector<double>::iterator itVec = v.begin(); itVec != v.end(); ++itVec )
        cfgFile << " " << ( *itVec );
      if ( parIdx < ( *it )->getFunctionNumParam( topLevel ) - 1 )
        cfgFile << ", ";
      else
        cfgFile << " } }" << endl;
    }
  }

  if ( ( *it )->isDerivedWindow() )
  {
    for ( TParamIndex parIdx = 0; parIdx < ( *it )->getFunctionNumParam( DERIVED ); ++parIdx )
    {
      if ( parIdx == 0 )
      {
        cfgFile << OLDCFG_TAG_WNDW_SEMANTIC_MODULE << " " << levelToString( DERIVED );
        cfgFile << " " << ( *it )->getLevelFunction( DERIVED ) << " { ";
        cfgFile << ( *it )->getFunctionNumParam( DERIVED ) << ", ";
        cfgFile << "{ ";
      }
      vector<double> v = ( *it )->getFunctionParam( DERIVED, parIdx );
      cfgFile << v.size();
      for ( vector<double>::iterator itVec = v.begin(); itVec != v.end(); ++itVec )
        cfgFile << " " << ( *itVec );
      if ( parIdx < ( *it )->getFunctionNumParam( DERIVED ) - 1 )
        cfgFile << ", ";
      else
        cfgFile << " } }" << endl;
    }
  }

  for ( int levelIdx = WORKLOAD; levelIdx <= CPU; ++levelIdx )
  {
    if( ( *it )->isDerivedWindow() && ( levelIdx == THREAD || levelIdx == CPU ) )
      continue;

    TWindowLevel level = ( TWindowLevel ) levelIdx;
    for ( TParamIndex parIdx = 0; parIdx < ( *it )->getFunctionNumParam( level ); ++parIdx )
    {
      if ( parIdx == 0 )
      {
        cfgFile << OLDCFG_TAG_WNDW_SEMANTIC_MODULE << " " << levelToString( level );
        cfgFile << " " << ( *it )->getLevelFunction( level ) << " { ";
        cfgFile << ( *it )->getFunctionNumParam( level ) << ", ";
        cfgFile << "{ ";
      }
      vector<double> v = ( *it )->getFunctionParam( level, parIdx );
      cfgFile << v.size();
      for ( vector<double>::iterator itVec = v.begin(); itVec != v.end(); ++itVec )
        cfgFile << " " << ( *itVec );
      if ( parIdx < ( *it )->getFunctionNumParam( level ) - 1 )
        cfgFile << ", ";
      else
        cfgFile << " } }" << endl;
    }
  }
  for ( int levelIdx = COMPOSEWORKLOAD; levelIdx <= COMPOSECPU; ++levelIdx )
  {
    TWindowLevel level = ( TWindowLevel ) levelIdx;
    for ( TParamIndex parIdx = 0; parIdx < ( *it )->getFunctionNumParam( level ); ++parIdx )
    {
      if ( parIdx == 0 )
      {
        cfgFile << OLDCFG_TAG_WNDW_SEMANTIC_MODULE << " " << levelToString( level );
        cfgFile << " " << ( *it )->getLevelFunction( level ) << " { ";
        cfgFile << ( *it )->getFunctionNumParam( level ) << ", ";
        cfgFile << "{ ";
      }
      vector<double> v = ( *it )->getFunctionParam( level, parIdx );
      cfgFile << v.size();
      for ( vector<double>::iterator itVec = v.begin(); itVec != v.end(); ++itVec )
        cfgFile << " " << ( *itVec );
      if ( parIdx < ( *it )->getFunctionNumParam( level ) - 1 )
        cfgFile << ", ";
      else
        cfgFile << " } }" << endl;
    }
  }
}


string WindowFilterModule::tagCFG = OLDCFG_TAG_WNDW_FILTER_MODULE;

bool WindowFilterModule::parseLine( KernelConnection *whichKernel, istringstream& line,
                                    Trace *whichTrace,
                                    vector<Timeline *>& windows,
                                    vector<Histogram *>& histograms )
{
  string strTag;
  Filter *filter;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  getline( line, strTag, ' ' );          // Parameter type.

  filter = windows[ windows.size() - 1 ]->getFilter();

  if ( strTag.compare( OLDCFG_VAL_FILTER_OBJ_FROM ) == 0 )
  {
    return parseLineFilter<TObjectOrder>( line, [filter]( TObjectOrder fromObject ) { filter->insertCommFrom( fromObject - 1 ); } );
  }
  else if ( strTag.compare( OLDCFG_VAL_FILTER_OBJ_TO ) == 0 )
  {
    return parseLineFilter<TObjectOrder>( line, [filter]( TObjectOrder toObject ) { filter->insertCommTo( toObject - 1 ); } );
  }
  else if ( strTag.compare( OLDCFG_VAL_FILTER_COM_TAG ) == 0 )
  {
    return parseLineFilter<TCommTag>( line, [filter]( TCommTag commTag ) { filter->insertCommTag( commTag ); } );
  }
  else if ( strTag.compare( OLDCFG_VAL_FILTER_COM_SIZE ) == 0 )
  {
    return parseLineFilter<TCommSize>( line, [filter]( TCommSize commSize ) { filter->insertCommSize( commSize ); } );
  }
  else if ( strTag.compare( OLDCFG_VAL_FILTER_COM_BW ) == 0 )
  {
    return parseLineFilter<TSemanticValue>( line, [filter]( TSemanticValue bandWidth ) { filter->insertBandWidth( bandWidth ); } );
  }
  else if ( strTag.compare( OLDCFG_VAL_FILTER_EVT_TYPE ) == 0 )
  {
    return parseLineFilter<TEventType>( line, []( TEventType eventType ) { eventTypeSymbolPicker.insert( eventType ); } );
  }
  else if ( strTag.compare( OLDCFG_VAL_FILTER_EVT_VALUE ) == 0 )
  {
    return parseLineFilter<TSemanticValue>( line, []( TSemanticValue eventValue ) { eventValueSymbolPicker.insert( eventValue ); } );
  }
  else if ( strTag.compare( CFG_VAL_FILTER_EVT_TYPE_LABEL ) == 0 )
  {
    return parseLineFilter<std::string>( line, []( std::string typeLabel ) { eventTypeSymbolPicker.insert( typeLabel ); } );
  }
  else if ( strTag.compare( CFG_VAL_FILTER_EVT_VALUE_LABEL ) == 0 )
  {
    return parseLineFilter<std::string>( line, []( std::string valueLabel ) { eventValueSymbolPicker.insert( valueLabel ); } );
  }

  return true;
}

void WindowFilterModule::printLine( ofstream& cfgFile,
                                    const vector<Timeline *>::const_iterator it )
{
  Filter *filter = ( *it )->getFilter();
  const EventLabels& labels = ( *it )->getTrace()->getEventLabels();
  vector<TObjectOrder> objVec;
  vector<TCommTag> tagVec;
  vector<TCommSize> sizeVec;
  vector<TSemanticValue> bwVec;
  vector<TEventType> typeVec;
  vector<TSemanticValue> valueVec;

  filter->getCommFrom( objVec );
  if ( objVec.begin() != objVec.end() )
  {
    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << OLDCFG_VAL_FILTER_OBJ_FROM << " ";
    cfgFile << objVec.size();
    for ( vector<TObjectOrder>::iterator itObj = objVec.begin();
          itObj != objVec.end(); ++itObj )
    {
      cfgFile << " " << ( *itObj ) + 1;
    }
    cfgFile << endl;
  }

  objVec.clear();
  filter->getCommTo( objVec );
  if ( objVec.begin() != objVec.end() )
  {
    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << OLDCFG_VAL_FILTER_OBJ_TO << " ";
    cfgFile << objVec.size();
    for ( vector<TObjectOrder>::iterator itObj = objVec.begin();
          itObj != objVec.end(); ++itObj )
    {
      cfgFile << " " << ( *itObj ) + 1;
    }
    cfgFile << endl;
  }

  filter->getCommTag( tagVec );
  if ( tagVec.begin() != tagVec.end() )
  {
    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << OLDCFG_VAL_FILTER_COM_TAG << " ";
    cfgFile << tagVec.size();
    for ( vector<TCommTag>::iterator itTag = tagVec.begin();
          itTag != tagVec.end(); ++itTag )
    {
      cfgFile << " " << ( *itTag );
    }
    cfgFile << endl;
  }

  filter->getCommSize( sizeVec );
  if ( sizeVec.begin() != sizeVec.end() )
  {
    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << OLDCFG_VAL_FILTER_COM_SIZE << " ";
    cfgFile << sizeVec.size();
    for ( vector<TCommSize>::iterator itSize = sizeVec.begin();
          itSize != sizeVec.end(); ++itSize )
    {
      cfgFile << " " << ( *itSize );
    }
    cfgFile << endl;
  }

  filter->getBandWidth( bwVec );
  if ( bwVec.begin() != bwVec.end() )
  {
    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << OLDCFG_VAL_FILTER_COM_BW << " ";
    cfgFile << bwVec.size();
    for ( vector<TSemanticValue>::iterator itBW = bwVec.begin();
          itBW != bwVec.end(); ++itBW )
    {
      cfgFile << " " << ( *itBW );
    }
    cfgFile << endl;
  }

  filter->getEventType( typeVec );
  if ( typeVec.begin() != typeVec.end() )
  {
    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << OLDCFG_VAL_FILTER_EVT_TYPE << " ";
    cfgFile << typeVec.size();
    for ( vector<TEventType>::iterator itType = typeVec.begin();
          itType != typeVec.end(); ++itType )
    {
      cfgFile << " " << ( *itType );
    }
    cfgFile << endl;

    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << CFG_VAL_FILTER_EVT_TYPE_LABEL << " ";
    cfgFile << typeVec.size();
    for ( vector<TEventType>::iterator itType = typeVec.begin();
          itType != typeVec.end(); ++itType )
    {
      string tmpTypeLabel;
      labels.getEventTypeLabel( ( *itType ), tmpTypeLabel );
      cfgFile << " \"" << tmpTypeLabel << "\"";
    }
    cfgFile << endl;

  }

  filter->getEventValue( valueVec );
  if ( valueVec.begin() != valueVec.end() )
  {
    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << OLDCFG_VAL_FILTER_EVT_VALUE << " ";
    cfgFile << valueVec.size();
    for ( vector<TSemanticValue>::iterator itValue = valueVec.begin();
          itValue != valueVec.end(); ++itValue )
    {
      cfgFile << " " << ( *itValue );
    }
    cfgFile << endl;

    cfgFile << OLDCFG_TAG_WNDW_FILTER_MODULE << " " << CFG_VAL_FILTER_EVT_VALUE_LABEL << " ";
    cfgFile << valueVec.size();
    for ( vector<TSemanticValue>::iterator itValue = valueVec.begin();
          itValue != valueVec.end(); ++itValue )
    {
      string tmpValueLabel;
      double tmpIntpart;
      if( std::modf( *itValue, &tmpIntpart) == 0.0 )
      {
        if ( typeVec.begin() != typeVec.end() )
        {
          for ( vector<TEventType>::iterator itType = typeVec.begin();
                itType != typeVec.end(); ++itType )
          {
            if( labels.getEventValueLabel( ( *itType ), ( *itValue ), tmpValueLabel ) )
              break;
          }
        }
        else
        {
          labels.getEventValueLabel( ( *itValue ), tmpValueLabel );
        }
      }
      else
        tmpValueLabel = EventLabels::unknownLabel;

      cfgFile << " \"" << tmpValueLabel << "\"";
    }
    cfgFile << endl;

  }
}


string WindowFilterLogical::tagCFG = OLDCFG_TAG_WNDW_LOGICAL_FILTERED;

bool WindowFilterLogical::parseLine( KernelConnection *whichKernel, istringstream& line,
                                     Trace *whichTrace,
                                     vector<Timeline *>& windows,
                                     vector<Histogram *>& histograms )
{
  string strBool;
  Filter *filter;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  getline( line, strBool, ' ' );

  filter = windows[ windows.size() - 1 ]->getFilter();

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    filter->setLogical( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    filter->setLogical( true );
  else
    return false;

  return true;
}

void WindowFilterLogical::printLine( ofstream& cfgFile,
                                     const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_LOGICAL_FILTERED << " ";
  if ( ( *it )->getFilter()->getLogical() )
    cfgFile << OLDCFG_VAL_TRUE;
  else
    cfgFile << OLDCFG_VAL_FALSE;
  cfgFile << endl;
}


string WindowFilterPhysical::tagCFG = OLDCFG_TAG_WNDW_PHYSICAL_FILTERED;

bool WindowFilterPhysical::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strBool;
  Filter *filter;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  getline( line, strBool, ' ' );

  filter = windows[ windows.size() - 1 ]->getFilter();

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    filter->setPhysical( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    filter->setPhysical( true );
  else
    return false;

  return true;
}

void WindowFilterPhysical::printLine( ofstream& cfgFile,
                                      const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_PHYSICAL_FILTERED << " ";
  if ( ( *it )->getFilter()->getPhysical() )
    cfgFile << OLDCFG_VAL_TRUE;
  else
    cfgFile << OLDCFG_VAL_FALSE;
  cfgFile << endl;
}


string WindowFilterBoolOpFromTo::tagCFG = OLDCFG_TAG_WNDW_FROMTO;

bool WindowFilterBoolOpFromTo::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string strBool;
  Filter *filter;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  filter = windows[ windows.size() - 1 ]->getFilter();

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    filter->setOpFromToOr();
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    filter->setOpFromToAnd();
  else
    return false;

  return true;
}

void WindowFilterBoolOpFromTo::printLine( ofstream& cfgFile,
    const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_FROMTO << " ";
  if ( ( *it )->getFilter()->getOpFromTo() )
    cfgFile << OLDCFG_VAL_TRUE;
  else
    cfgFile << OLDCFG_VAL_FALSE;
  cfgFile << endl;
}


string WindowFilterBoolOpTagSize::tagCFG = OLDCFG_TAG_WNDW_COMM_TAGSIZE;

bool WindowFilterBoolOpTagSize::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string strBool;
  Filter *filter;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  getline( line, strBool, ' ' );

  filter = windows[ windows.size() - 1 ]->getFilter();

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    filter->setOpTagSizeOr();
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    filter->setOpTagSizeAnd();
  else
    return false;

  return true;
}

void WindowFilterBoolOpTagSize::printLine( ofstream& cfgFile,
    const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_COMM_TAGSIZE << " ";
  if ( ( *it )->getFilter()->getOpTagSize() )
    cfgFile << OLDCFG_VAL_TRUE;
  else
    cfgFile << OLDCFG_VAL_FALSE;
  cfgFile << endl;
}


string WindowFilterBoolOpTypeVal::tagCFG = OLDCFG_TAG_WNDW_TYPEVAL;

bool WindowFilterBoolOpTypeVal::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string strBool;
  Filter *filter;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  if ( windows[ windows.size() - 1 ]->isDerivedWindow() )
    return true;

  getline( line, strBool, ' ' );

  filter = windows[ windows.size() - 1 ]->getFilter();

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    filter->setOpTypeValueOr();
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    filter->setOpTypeValueAnd();
  else
    return false;

  return true;
}

void WindowFilterBoolOpTypeVal::printLine( ofstream& cfgFile,
    const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_TYPEVAL << " ";
  if ( ( *it )->getFilter()->getOpTypeValue() )
    cfgFile << OLDCFG_VAL_TRUE;
  else
    cfgFile << OLDCFG_VAL_FALSE;
  cfgFile << endl;
}


string WindowOpen::tagCFG = OLDCFG_TAG_WNDW_OPEN;

bool WindowOpen::parseLine( KernelConnection *whichKernel, istringstream& line,
                            Trace *whichTrace,
                            vector<Timeline *>& windows,
                            vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE ) == 0 )
    windows[ windows.size() - 1 ]->setShowWindow( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE ) == 0 )
    windows[ windows.size() - 1 ]->setShowWindow( true );
  else
    return false;

  return true;
}

void WindowOpen::printLine( ofstream& cfgFile,
                            const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_OPEN << " ";
  if ( ( *it )->getShowWindow() && !( *it )->getUsedByHistogram() )
  {
    if ( ( *it )->getChild() != nullptr )
      cfgFile << OLDCFG_VAL_FALSE;
    else
      cfgFile << OLDCFG_VAL_TRUE;
  }
  else
    cfgFile << OLDCFG_VAL_FALSE;
  cfgFile << endl;
}


string WindowDrawMode::tagCFG = OLDCFG_TAG_WNDW_DRAW_MODE;

bool WindowDrawMode::parseLine( KernelConnection *whichKernel,
                                istringstream& line,
                                Trace *whichTrace,
                                vector<Timeline *>& windows,
                                vector<Histogram *>& histograms )
{
  string strMode;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strMode, ' ' );

  if ( drawModeCodes.find( strMode ) != drawModeCodes.end() )
    windows[ windows.size() - 1 ]->setDrawModeTime( drawModeCodes[ strMode ] );

  return true;
}


void WindowDrawMode::printLine( ofstream& cfgFile,
                                const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_DRAW_MODE << " ";

  if ( drawModeTags.find( ( *it )->getDrawModeTime() ) != drawModeTags.end() )
    cfgFile << drawModeTags[ ( *it )->getDrawModeTime() ];

  cfgFile << endl;
}


string WindowDrawModeRows::tagCFG = OLDCFG_TAG_WNDW_DRAW_MODE_ROWS;

bool WindowDrawModeRows::parseLine( KernelConnection *whichKernel, istringstream& line,
                                    Trace *whichTrace,
                                    vector<Timeline *>& windows,
                                    vector<Histogram *>& histograms )
{
  string strMode;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strMode, ' ' );

  if ( drawModeCodes.find( strMode ) != drawModeCodes.end() )
    windows[ windows.size() - 1 ]->setDrawModeObject( drawModeCodes[ strMode ] );

  return true;
}

void WindowDrawModeRows::printLine( ofstream& cfgFile,
                                    const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_DRAW_MODE_ROWS << " ";

  if ( drawModeTags.find( ( *it )->getDrawModeObject() ) != drawModeTags.end() )
    cfgFile << drawModeTags[ ( *it )->getDrawModeObject() ];

  cfgFile << endl;
}


string WindowPixelSize::tagCFG = OLDCFG_TAG_WNDW_PIXEL_SIZE;

bool WindowPixelSize::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strSize;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strSize, ' ' );
  istringstream tmpStream( strSize );
  PRV_UINT16 size;

  if ( !( tmpStream >> size ) )
    return false;

  windows[ windows.size() - 1 ]->setPixelSize( size );

  return true;
}

void WindowPixelSize::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_PIXEL_SIZE << " " << (*it)->getPixelSize() << endl;
}


string WindowLabelsToDraw::tagCFG = OLDCFG_TAG_WNDW_LABELS_TO_DRAW;

bool WindowLabelsToDraw::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strLabels;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strLabels, ' ' );
  istringstream tmpStream( strLabels );
  PRV_UINT16 labels;

  if ( !( tmpStream >> labels ) )
    return false;

  windows[ windows.size() - 1 ]->setObjectLabels( ( TObjectLabels )labels );

  return true;
}

void WindowLabelsToDraw::printLine( ofstream& cfgFile,
                                 const vector<Timeline *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_WNDW_LABELS_TO_DRAW << " " << static_cast< int > ( (*it)->getObjectLabels() ) << endl;
}


string WindowPunctualColorWindow::tagCFG = OLDCFG_TAG_WNDW_PUNCTUAL_COLOR_WIN;

bool WindowPunctualColorWindow::parseLine( KernelConnection *whichKernel, istringstream& line,
                                           Trace *whichTrace,
                                           vector<Timeline *>& windows,
                                           vector<Histogram *>& histograms )
{
  string strID;
  PRV_UINT16 id;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strID );
  istringstream tmpStream( strID );
  if ( !( tmpStream >> id ) )
    return false;

  if ( windows[ id - 1 ] == nullptr )
    return false;

  windows[ windows.size() - 1 ]->setPunctualColorWindow( windows[ id - 1 ] );

  return true;
}

void WindowPunctualColorWindow::printLine( ofstream& cfgFile,
                                           const vector<Timeline *>& allWindows,
                                           const vector<Timeline *>::const_iterator it )
{
  if( (*it)->isPunctualColorSet() && (*it)->getPunctualColorWindow() != nullptr )
  {
    cfgFile << OLDCFG_TAG_WNDW_PUNCTUAL_COLOR_WIN << " " \
            << CFGLoader::findWindowBackwards( (*it)->getPunctualColorWindow(), allWindows, it ) + 1 << endl;
  }
}


string WindowSynchronize::tagCFG = OLDCFG_TAG_WNDW_SYNCHRONIZE;

bool WindowSynchronize::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string strGroupID;
  TGroupId groupID;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;

  getline( line, strGroupID );
  istringstream tmpStream( strGroupID );
  if ( !( tmpStream >> groupID ) )
    return false;

  TGroupId realGroupID;
  if( syncRealGroup.find( groupID ) == syncRealGroup.end() )
  {
    realGroupID = SyncWindows::getInstance()->newGroup();
    syncRealGroup[ groupID ] = realGroupID;
  }
  else
    realGroupID = syncRealGroup[ groupID ];

  windows[ windows.size() - 1 ]->addToSyncGroup( realGroupID );

  return true;
}

void WindowSynchronize::printLine( ofstream& cfgFile,
                                   const vector<Timeline *>::const_iterator it )
{
  if( (*it)->isSync() )
  {
    TGroupId realGroupID;
    if( syncRealGroup.find( (*it)->getSyncGroup() ) == syncRealGroup.end() )
    {
      realGroupID = ++lastSyncGroupUsed;
      syncRealGroup[ (*it)->getSyncGroup() ] = realGroupID;
    }
    else
      realGroupID = syncRealGroup[ (*it)->getSyncGroup() ];

    cfgFile << OLDCFG_TAG_WNDW_SYNCHRONIZE << " " << realGroupID << endl;
  }
}


string Analyzer2DCreate::tagCFG = OLDCFG_TAG_AN2D_NEW;

bool Analyzer2DCreate::parseLine( KernelConnection *whichKernel,
                                  istringstream& line,
                                  Trace *whichTrace,
                                  vector<Timeline *>& windows,
                                  vector<Histogram *>& histograms )
{
  if ( histograms[ histograms.size() - 1 ] == nullptr )
  {
    histograms[ histograms.size() - 1 ] = Histogram::create( whichKernel );
  }
  else
  {
    histograms.push_back( Histogram::create( whichKernel ) );
  }

  isWindowTag = false; // CFG4D
  histograms[ histograms.size() - 1 ]->setCFG4DMode( false );

  return true;
}

void Analyzer2DCreate::printLine( ofstream& cfgFile,
                                  const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_NEW << endl;
}


string Analyzer2DName::tagCFG = OLDCFG_TAG_AN2D_NAME;

bool Analyzer2DName::parseLine( KernelConnection *whichKernel,
                                istringstream& line,
                                Trace *whichTrace,
                                vector<Timeline *>& windows,
                                vector<Histogram *>& histograms )
{
  string strName;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strName );
  histograms[ histograms.size() - 1 ]->setName( strName );

  return true;
}

void Analyzer2DName::printLine( ofstream& cfgFile,
                                const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_NAME << " " << ( *it )->getName() << endl;
}


string Analyzer2DX::tagCFG = OLDCFG_TAG_AN2D_X;

bool Analyzer2DX::parseLine( KernelConnection *whichKernel, istringstream& line,
                             Trace *whichTrace,
                             vector<Timeline *>& windows,
                             vector<Histogram *>& histograms )
{
  string strX;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strX, ' ' );
  istringstream tmpStream( strX );
  PRV_UINT16 x;

  if ( !( tmpStream >> x ) )
    return false;

  histograms[ histograms.size() - 1 ]->setPosX( x );

  return true;
}

void Analyzer2DX::printLine( ofstream& cfgFile,
                             const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_X << " " << ( *it )->getPosX() << endl;
}


string Analyzer2DY::tagCFG = OLDCFG_TAG_AN2D_Y;

bool Analyzer2DY::parseLine( KernelConnection *whichKernel, istringstream& line,
                             Trace *whichTrace,
                             vector<Timeline *>& windows,
                             vector<Histogram *>& histograms )
{
  string strY;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strY, ' ' );
  istringstream tmpStream( strY );
  PRV_UINT16 y;

  if ( !( tmpStream >> y ) )
    return false;

  histograms[ histograms.size() - 1 ]->setPosY( y );

  return true;
}

void Analyzer2DY::printLine( ofstream& cfgFile,
                             const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_Y << " " << ( *it )->getPosY() << endl;
}


string Analyzer2DWidth::tagCFG = OLDCFG_TAG_AN2D_WIDTH;

bool Analyzer2DWidth::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strWidth;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strWidth, ' ' );
  istringstream tmpStream( strWidth );
  PRV_UINT16 width;

  if ( !( tmpStream >> width ) )
    return false;

  histograms[ histograms.size() - 1 ]->setWidth( width );

  return true;
}

void Analyzer2DWidth::printLine( ofstream& cfgFile,
                                 const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_WIDTH << " " << ( *it )->getWidth() << endl;
}


string Analyzer2DHeight::tagCFG = OLDCFG_TAG_AN2D_HEIGHT;

bool Analyzer2DHeight::parseLine( KernelConnection *whichKernel,
                                  istringstream& line,
                                  Trace *whichTrace,
                                  vector<Timeline *>& windows,
                                  vector<Histogram *>& histograms )
{
  string strHeight;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strHeight, ' ' );
  istringstream tmpStream( strHeight );
  PRV_UINT16 height;

  if ( !( tmpStream >> height ) )
    return false;

  histograms[ histograms.size() - 1 ]->setHeight( height );

  return true;
}

void Analyzer2DHeight::printLine( ofstream& cfgFile,
                                  const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_HEIGHT << " " << ( *it )->getHeight() << endl;
}


string Analyzer2DControlWindow::tagCFG = OLDCFG_TAG_AN2D_CONTROL_WINDOW;

bool Analyzer2DControlWindow::parseLine( KernelConnection *whichKernel,
                                         istringstream& line,
                                         Trace *whichTrace,
                                         vector<Timeline *>& windows,
                                         vector<Histogram *>& histograms )
{
  string strIndexControlWindow;
  PRV_UINT32 indexControlWindow;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strIndexControlWindow );
  istringstream tmpWindow( strIndexControlWindow );
  if ( !( tmpWindow >> indexControlWindow ) )
    return false;

  if ( indexControlWindow > windows.size() ||
       windows[ indexControlWindow - 1 ] == nullptr )
    return false;

  if( histograms[ histograms.size() - 1 ]->getDataWindow() != nullptr &&
      !LoadedWindows::validDataWindow( histograms[ histograms.size() - 1 ]->getDataWindow(),
                                       windows[ indexControlWindow - 1 ] ) )
    return false;

  if( histograms[ histograms.size() - 1 ]->getExtraControlWindow() != nullptr &&
      !LoadedWindows::validDataWindow( windows[ indexControlWindow - 1 ],
                                       histograms[ histograms.size() - 1 ]->getExtraControlWindow() ) )
    return false;

  histograms[ histograms.size() - 1 ]->setControlWindow( windows[ indexControlWindow - 1 ] );

  return true;
}

void Analyzer2DControlWindow::printLine( ofstream& cfgFile,
    const vector<Timeline *>& allWindows,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_CONTROL_WINDOW << " ";
  cfgFile << CFGLoader::findWindow( ( *it )->getControlWindow(), allWindows ) + 1;
  cfgFile << endl;
}


string Analyzer2DDataWindow::tagCFG = OLDCFG_TAG_AN2D_DATA_WINDOW;

bool Analyzer2DDataWindow::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strIndexDataWindow;
  PRV_UINT32 indexDataWindow;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strIndexDataWindow );
  istringstream tmpWindow( strIndexDataWindow );
  if ( !( tmpWindow >> indexDataWindow ) )
    return false;

  if ( indexDataWindow > windows.size() ||
       windows[ indexDataWindow - 1 ] == nullptr )
    return false;

  if( histograms[ histograms.size() - 1 ]->getControlWindow() != nullptr &&
      !LoadedWindows::validDataWindow( windows[ indexDataWindow - 1 ],
                                       histograms[ histograms.size() - 1 ]->getControlWindow() ) )
    return false;

  if( histograms[ histograms.size() - 1 ]->getExtraControlWindow() != nullptr &&
      !LoadedWindows::validDataWindow( windows[ indexDataWindow - 1 ],
                                       histograms[ histograms.size() - 1 ]->getExtraControlWindow() ) )
    return false;

  histograms[ histograms.size() - 1 ]->setDataWindow( windows[ indexDataWindow - 1 ] );

  return true;
}

void Analyzer2DDataWindow::printLine( ofstream& cfgFile,
                                      const vector<Timeline *>& allWindows,
                                      const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_DATA_WINDOW << " ";
  cfgFile << CFGLoader::findWindow( ( *it )->getDataWindow(), allWindows ) + 1;
  cfgFile << endl;
}


string Analyzer2DStatistic::tagCFG = OLDCFG_TAG_AN2D_STATISTIC;

bool Analyzer2DStatistic::parseLine( KernelConnection *whichKernel, istringstream& line,
                                     Trace *whichTrace,
                                     vector<Timeline *>& windows,
                                     vector<Histogram *>& histograms )
{
  string strStatistic;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strStatistic );

  if ( !histograms[ histograms.size() - 1 ]->getCalculateAll() )
    histograms[ histograms.size() - 1 ]->pushbackStatistic( strStatistic );

  histograms[ histograms.size() - 1 ]->setCurrentStat( strStatistic );

  return true;
}

void Analyzer2DStatistic::printLine( ofstream& cfgFile,
                                     const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_STATISTIC << " ";
  cfgFile << ( *it )->getCurrentStat() << endl;
}


string Analyzer2DCalculateAll::tagCFG = OLDCFG_TAG_AN2D_CALCULATE_ALL;

bool Analyzer2DCalculateAll::parseLine( KernelConnection *whichKernel, istringstream& line,
                                        Trace *whichTrace,
                                        vector<Timeline *>& windows,
                                        vector<Histogram *>& histograms )
{
  string strBoolAll;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  histograms[ histograms.size() - 1 ]->setCalculateAll( true );
  /*  getline( line, strBoolAll, ' ' );

    if ( strBoolAll.compare( OLDCFG_VAL_TRUE2 ) == 0 )
      histograms[ histograms.size() - 1 ]->setCalculateAll( true );
    else if ( strBoolAll.compare( OLDCFG_VAL_FALSE2 ) == 0 )
      histograms[ histograms.size() - 1 ]->setCalculateAll( false );
    else
      return false;*/

  return true;
}

void Analyzer2DCalculateAll::printLine( ofstream& cfgFile,
                                        const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_CALCULATE_ALL << " ";
  if ( ( *it )->getCalculateAll() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DHideColumns::tagCFG = OLDCFG_TAG_AN2D_HIDE_COLS;

bool Analyzer2DHideColumns::parseLine( KernelConnection *whichKernel, istringstream& line,
                                       Trace *whichTrace,
                                       vector<Timeline *>& windows,
                                       vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setHideColumns( true );
  else if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setHideColumns( false );
  else
    return false;

  return true;
}

void Analyzer2DHideColumns::printLine( ofstream& cfgFile,
                                       const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_HIDE_COLS << " ";
  if ( ( *it )->getHideColumns() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DHorizontal::tagCFG = OLDCFG_TAG_AN2D_HORIZONTAL;

bool Analyzer2DHorizontal::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_HORIZONTAL ) == 0 )
    histograms[ histograms.size() - 1 ]->setHorizontal( true );
  else if ( strBool.compare( OLDCFG_VAL_VERTICAL ) == 0 )
    histograms[ histograms.size() - 1 ]->setHorizontal( false );
  else
    return false;

  return true;
}

void Analyzer2DHorizontal::printLine( ofstream& cfgFile,
                                      const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_HORIZONTAL << " ";
  if ( ( *it )->getHorizontal() )
    cfgFile << OLDCFG_VAL_HORIZONTAL;
  else
    cfgFile << OLDCFG_VAL_VERTICAL;
  cfgFile << endl;
}


string Analyzer2DColor::tagCFG = OLDCFG_TAG_AN2D_COLOR;

bool Analyzer2DColor::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setShowColor( true );
  else if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setShowColor( false );

  return true;
}

void Analyzer2DColor::printLine( ofstream& cfgFile,
                                 const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_COLOR << " ";
  if ( ( *it )->getShowColor() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DSemanticColor::tagCFG = OLDCFG_TAG_AN2D_SEMANTIC_COLOR;

bool Analyzer2DSemanticColor::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setFirstRowColored( true );
  else if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setFirstRowColored( false );

  return true;
}

void Analyzer2DSemanticColor::printLine( ofstream& cfgFile,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_SEMANTIC_COLOR << " ";
  if ( ( *it )->getFirstRowColored() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DZoom::tagCFG = OLDCFG_TAG_AN2D_ZOOM;

bool Analyzer2DZoom::parseLine( KernelConnection *whichKernel, istringstream& line,
                                Trace *whichTrace,
                                vector<Timeline *>& windows,
                                vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_ENABLED ) == 0 )
    histograms[ histograms.size() - 1 ]->setZoom( true );
  else if ( strBool.compare( OLDCFG_VAL_DISABLED ) == 0 )
    histograms[ histograms.size() - 1 ]->setZoom( false );

  return true;
}

void Analyzer2DZoom::printLine( ofstream& cfgFile,
                                const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_ZOOM << " ";
  if ( ( *it )->getZoom() )
    cfgFile << OLDCFG_VAL_ENABLED;
  else
    cfgFile << OLDCFG_VAL_DISABLED;
  cfgFile << endl;
}


string Analyzer2DAccumulator::tagCFG = OLDCFG_TAG_AN2D_ACCUMULATOR;

bool Analyzer2DAccumulator::parseLine( KernelConnection *whichKernel, istringstream& line,
                                        Trace *whichTrace,
                                        vector<Timeline *>& windows,
                                        vector<Histogram *>& histograms )
{
  string strAccumulator;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  return true;
}

void Analyzer2DAccumulator::printLine( ofstream& cfgFile,
                                       const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_ACCUMULATOR << " ";
  if ( ( *it )->isCommunicationStat( ( *it )->getCurrentStat() ) )
    cfgFile << levelToStringHisto( static_cast<TWindowLevel>( ( *it )->getControlWindow()->getLevel() ) );
  else
    cfgFile << OLDCFG_VAL_AN2D_ACCUM_SEMANTIC;
  cfgFile << endl;
}



string Analyzer2DAccumulateByControlWindow::tagCFG = "";

bool Analyzer2DAccumulateByControlWindow::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    {}
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    {}
  else
    return false;

  return true;
}

void Analyzer2DAccumulateByControlWindow::printLine( ofstream& cfgFile,
    const vector<Histogram *>::const_iterator it )
{
}


string Analyzer2DSortCols::tagCFG = OLDCFG_TAG_AN2D_SORTCOLS;

bool Analyzer2DSortCols::parseLine( KernelConnection *whichKernel, istringstream& line,
                                    Trace *whichTrace,
                                    vector<Timeline *>& windows,
                                    vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
  {
    histograms[ histograms.size() - 1 ]->setSemanticSortColumns( true );
  }
  else if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
  {
    histograms[ histograms.size() - 1 ]->setSemanticSortColumns( false );
  }
  else
    return false;

  return true;
}

void Analyzer2DSortCols::printLine( ofstream& cfgFile,
                                    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_SORTCOLS << " ";
  if ( ( *it )->getSemanticSortColumns() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DSortCriteria::tagCFG = OLDCFG_TAG_AN2D_SORTCRITERIA;

bool Analyzer2DSortCriteria::parseLine( KernelConnection *whichKernel, istringstream& line,
                                        Trace *whichTrace,
                                        vector<Timeline *>& windows,
                                        vector<Histogram *>& histograms )
{
  string strSortCriteria;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strSortCriteria );

  if ( strSortCriteria.compare( OLDCFG_VAL_SORT_AVERAGE ) == 0 )
    histograms[ histograms.size() - 1 ]->setSemanticSortCriteria( THistoSortCriteria::AVERAGE );
  else if ( strSortCriteria.compare( OLDCFG_VAL_SORT_TOTAL ) == 0 )
    histograms[ histograms.size() - 1 ]->setSemanticSortCriteria( THistoSortCriteria::TOTAL );
  else if ( strSortCriteria.compare( OLDCFG_VAL_SORT_MAXIMUM ) == 0 )
    histograms[ histograms.size() - 1 ]->setSemanticSortCriteria( THistoSortCriteria::MAXIMUM );
  else if ( strSortCriteria.compare( OLDCFG_VAL_SORT_MINIMUM ) == 0 )
    histograms[ histograms.size() - 1 ]->setSemanticSortCriteria( THistoSortCriteria::MINIMUM );
  else if ( strSortCriteria.compare( OLDCFG_VAL_SORT_STDEV ) == 0 )
    histograms[ histograms.size() - 1 ]->setSemanticSortCriteria( THistoSortCriteria::STDEV );
  else if ( strSortCriteria.compare( OLDCFG_VAL_SORT_AVGDIVMAX ) == 0 )
    histograms[ histograms.size() - 1 ]->setSemanticSortCriteria( THistoSortCriteria::AVGDIVMAX );
  else if ( strSortCriteria.compare( OLDCFG_VAL_SORT_CUSTOM ) == 0 )
  {
    histograms[ histograms.size() - 1 ]->setSemanticSortCriteria( THistoSortCriteria::CUSTOM );
    histograms[ histograms.size() - 1 ]->setSemanticSortColumns( false );
  }
  else
    return false;

  return true;
}

void Analyzer2DSortCriteria::printLine( ofstream& cfgFile,
                                        const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_SORTCRITERIA << " ";
  switch ( ( *it )->getSemanticSortCriteria() )
  {
    case THistoSortCriteria::AVERAGE:
      cfgFile << OLDCFG_VAL_SORT_AVERAGE;
      break;
    case THistoSortCriteria::TOTAL:
      cfgFile << OLDCFG_VAL_SORT_TOTAL;
      break;
    case THistoSortCriteria::MAXIMUM:
      cfgFile << OLDCFG_VAL_SORT_MAXIMUM;
      break;
    case THistoSortCriteria::MINIMUM:
      cfgFile << OLDCFG_VAL_SORT_MINIMUM;
      break;
    case THistoSortCriteria::STDEV:
      cfgFile << OLDCFG_VAL_SORT_STDEV;
      break;
    case THistoSortCriteria::AVGDIVMAX:
      cfgFile << OLDCFG_VAL_SORT_AVGDIVMAX;
      break;
    case THistoSortCriteria::CUSTOM:
      cfgFile << OLDCFG_VAL_SORT_CUSTOM;
      break;
    default:
      break;
  }
  cfgFile << endl;
}


string Analyzer2DSortReverse::tagCFG = OLDCFG_TAG_AN2D_SORTREVERSE;

bool Analyzer2DSortReverse::parseLine( KernelConnection *whichKernel, istringstream& line,
                                       Trace *whichTrace,
                                       vector<Timeline *>& windows,
                                       vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
  {
    histograms[ histograms.size() - 1 ]->setSemanticSortReverse( true );
  }
  else if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
  {
    histograms[ histograms.size() - 1 ]->setSemanticSortReverse( false );
  }
  else
    return false;

  return true;
}

void Analyzer2DSortReverse::printLine( ofstream& cfgFile,
                                       const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_SORTREVERSE << " ";
  if ( ( *it )->getSemanticSortReverse() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


/*
 Number_of_parameters Parameter1 ... ParameterN
*/


string Analyzer2DParameters::tagCFG = OLDCFG_TAG_AN2D_PARAMETERS;

bool Analyzer2DParameters::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strNumParams, strValue;
  PRV_UINT16 numParams;
  double dataValue;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strNumParams, ' ' ); // Number of following parameters.
  istringstream tmpNumParams( strNumParams );

  if ( !( tmpNumParams >> numParams ) )
    return false;

  for ( PRV_UINT16 ii = 0; ii < numParams; ii++ )
  {
    getline( line, strValue, ' ' );
    istringstream tmpValue( strValue );
    if ( !( tmpValue >> dataValue ) )
      continue;
    if ( ii == 0 )
    {
      histograms[ histograms.size() - 1 ]->setDataMin( dataValue );
      //histograms[ histograms.size() - 1 ]->setCommSizeMin( ( TCommSize ) dataValue );
    }
    else if ( ii == 1 )
    {
      histograms[ histograms.size() - 1 ]->setDataMax( dataValue );
      //histograms[ histograms.size() - 1 ]->setCommSizeMax( ( TCommSize ) dataValue );
    }
    else if ( ii == 2 )
    {
      histograms[ histograms.size() - 1 ]->setBurstMin( dataValue );
      //histograms[ histograms.size() - 1 ]->setCommTagMin( ( TCommTag ) dataValue );
    }
    else if ( ii == 3 )
    {
      histograms[ histograms.size() - 1 ]->setBurstMax( dataValue );
      //histograms[ histograms.size() - 1 ]->setCommTagMax( ( TCommTag ) dataValue );
    }
  }

  return true;
}

void Analyzer2DParameters::printLine( ofstream& cfgFile,
                                      const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_PARAMETERS << " 4 ";
  cfgFile << ( *it )->getDataMin() << " ";
  cfgFile << ( *it )->getDataMax() << " ";
  cfgFile << ( *it )->getBurstMin() << " ";
  cfgFile << ( *it )->getBurstMax();
  cfgFile << endl;
}


string Analyzer2DAnalysisLimits::tagCFG = OLDCFG_TAG_AN2D_ANALYSISLIMITS;

bool Analyzer2DAnalysisLimits::parseLine( KernelConnection *whichKernel,
                                          istringstream& line,
                                          Trace *whichTrace,
                                          vector<Timeline *>& windows,
                                          vector<Histogram *>& histograms )
{
  string strLimit;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strLimit );

  if ( strLimit.compare( OLDCFG_VAL_LIMIT_ALLTRACE ) == 0 )
  {
    histograms[ histograms.size() - 1 ]->setWindowBeginTime( 0.0 );
    histograms[ histograms.size() - 1 ]->setWindowEndTime( whichTrace->getEndTime() );
  }
  else if ( strLimit.compare( OLDCFG_VAL_LIMIT_ALLWINDOW ) == 0 )
  {
    histograms[ histograms.size() - 1 ]->setWindowBeginTime( histograms[ histograms.size() - 1 ]->getControlWindow()->getWindowBeginTime() );
    histograms[ histograms.size() - 1 ]->setWindowEndTime( histograms[ histograms.size() - 1 ]->getControlWindow()->getWindowEndTime() );
  }
  else if ( strLimit.compare( OLDCFG_VAL_LIMIT_REGION ) == 0 )
  {
    // Not implemented yet
    histograms[ histograms.size() - 1 ]->setWindowBeginTime( 0.0 );
    histograms[ histograms.size() - 1 ]->setWindowEndTime( whichTrace->getEndTime() );
  }
  else
    return false;

  return true;
}

void Analyzer2DAnalysisLimits::printLine( ofstream& cfgFile,
                                          const SaveOptions& options,
                                          const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_ANALYSISLIMITS << " ";
  if ( options.histoAllTrace )
    cfgFile << OLDCFG_VAL_LIMIT_ALLTRACE;
  else
    cfgFile << OLDCFG_VAL_LIMIT_ALLWINDOW;
  cfgFile << endl;
}


string Analyzer2DRelativeTime::tagCFG = "";

bool Analyzer2DRelativeTime::parseLine( KernelConnection *whichKernel,
                                        istringstream& line,
                                        Trace *whichTrace,
                                        vector<Timeline *>& windows,
                                        vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    {}
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    {}
  else
    return false;

  return true;
}

void Analyzer2DRelativeTime::printLine( ofstream& cfgFile,
                                        const vector<Histogram *>::const_iterator it )
{
}


string Analyzer2DComputeYScale::tagCFG = OLDCFG_TAG_AN2D_COMPUTEYSCALE;

bool Analyzer2DComputeYScale::parseLine( KernelConnection *whichKernel,
                                         istringstream& line,
                                         Trace *whichTrace,
                                         vector<Timeline *>& windows,
                                         vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCompute2DScale( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCompute2DScale( true );
  else
    return false;

  return true;
}

void Analyzer2DComputeYScale::printLine( ofstream& cfgFile,
    const SaveOptions& options,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_COMPUTEYSCALE << " ";
  if ( ( *it )->getCompute2DScale() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DComputeYScaleZero::tagCFG = CFG_TAG_AN2D_COMPUTEYSCALE_ZERO;

bool Analyzer2DComputeYScaleZero::parseLine( KernelConnection *whichKernel,
                                             istringstream& line,
                                             Trace *whichTrace,
                                             vector<Timeline *>& windows,
                                             vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCompute2DScaleZero( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCompute2DScaleZero( true );
  else
    return false;

  return true;
}

void Analyzer2DComputeYScaleZero::printLine( ofstream& cfgFile,
                                             const SaveOptions& options,
                                             const vector<Histogram *>::const_iterator it )
{
  cfgFile << CFG_TAG_AN2D_COMPUTEYSCALE_ZERO << " ";
  if ( ( *it )->getCompute2DScaleZero() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DMinimum::tagCFG = OLDCFG_TAG_AN2D_MINIMUM;

bool Analyzer2DMinimum::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string strMinimum;
  THistogramLimit dataMinimum;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strMinimum );
  istringstream tmpValue( strMinimum );
  if ( !( tmpValue >> dataMinimum ) )
    return false;
  histograms[ histograms.size() - 1 ]->setControlMin( dataMinimum );

  return true;
}

void Analyzer2DMinimum::printLine( ofstream& cfgFile,
                                   const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_MINIMUM << " " << ( *it )->getControlMin() << endl;
}


string Analyzer2DMaximum::tagCFG = OLDCFG_TAG_AN2D_MAXIMUM;

bool Analyzer2DMaximum::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string strMaximum;
  THistogramLimit dataMaximum;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strMaximum );
  istringstream tmpValue( strMaximum );
  if ( !( tmpValue >> dataMaximum ) )
    return false;
  histograms[ histograms.size() - 1 ]->setControlMax( dataMaximum );

  return true;
}

void Analyzer2DMaximum::printLine( ofstream& cfgFile,
                                   const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_MAXIMUM << " " << ( *it )->getControlMax() << endl;
}




string  Analyzer2DDelta::tagCFG = OLDCFG_TAG_AN2D_DELTA;

bool Analyzer2DDelta::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strDelta;
  THistogramLimit dataDelta;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strDelta );
  istringstream tmpValue( strDelta );
  if ( !( tmpValue >> dataDelta ) )
    return false;
  histograms[ histograms.size() - 1 ]->setControlDelta( dataDelta );

  return true;
}

void Analyzer2DDelta::printLine( ofstream& cfgFile,
                                 const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_DELTA << " " << ( *it )->getControlDelta() << endl;
}


string  Analyzer2DUseCustomDelta::tagCFG = CFG_TAG_AN2D_USE_CUSTOM_DELTA;

bool Analyzer2DUseCustomDelta::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setUseCustomDelta( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setUseCustomDelta( true );
  else
    return false;

  return true;
}

void Analyzer2DUseCustomDelta::printLine( ofstream& cfgFile,
                                 const vector<Histogram *>::const_iterator it )
{
  cfgFile << CFG_TAG_AN2D_USE_CUSTOM_DELTA << " ";
  if ( ( *it )->getUseCustomDelta() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string  Analyzer2DNumColumns::tagCFG = CFG_TAG_AN2D_NUMCOLUMNS;

bool Analyzer2DNumColumns::parseLine( KernelConnection *whichKernel,
                                      istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strColumn;
  THistogramColumn dataColumn;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strColumn );
  istringstream tmpValue( strColumn );
  if ( !( tmpValue >> dataColumn ) )
    return false;
  histograms[ histograms.size() - 1 ]->setNumColumns( dataColumn );

  return true;
}

void Analyzer2DNumColumns::printLine( ofstream& cfgFile,
                                      const vector<Histogram *>::const_iterator it )
{
  cfgFile << CFG_TAG_AN2D_NUMCOLUMNS << " " << ( *it )->getNumColumns() << endl;
}


string Analyzer2DComputeGradient::tagCFG = OLDCFG_TAG_AN2D_COMPUTEGRADIENT;

bool Analyzer2DComputeGradient::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setComputeGradient( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setComputeGradient( true );
  else
    return false;

  return true;
}

void Analyzer2DComputeGradient::printLine( ofstream& cfgFile,
    const SaveOptions& options,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_COMPUTEGRADIENT << " ";
  if ( options.histoComputeGradient ||
       ( *it )->getComputeGradient() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}



string Analyzer2DMinimumGradient::tagCFG = OLDCFG_TAG_AN2D_MINIMUMGRADIENT;

bool Analyzer2DMinimumGradient::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string strMinimumGradient;
  THistogramLimit dataMinimumGradient;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strMinimumGradient );
  istringstream tmpValue( strMinimumGradient );
  if ( !( tmpValue >> dataMinimumGradient ) )
    return false;
  histograms[ histograms.size() - 1 ]->setMinGradient( dataMinimumGradient );

  return true;
}

void Analyzer2DMinimumGradient::printLine( ofstream& cfgFile,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_MINIMUMGRADIENT << " " << ( *it )->getMinGradient() << endl;
}


string Analyzer2DMaximumGradient::tagCFG = OLDCFG_TAG_AN2D_MAXIMUMGRADIENT;

bool Analyzer2DMaximumGradient::parseLine( KernelConnection *whichKernel,
                                           istringstream& line,
                                           Trace *whichTrace,
                                           vector<Timeline *>& windows,
                                           vector<Histogram *>& histograms )
{
  string strMaximumGradient;
  THistogramLimit dataMaximumGradient;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strMaximumGradient );
  istringstream tmpValue( strMaximumGradient );
  if ( !( tmpValue >> dataMaximumGradient ) )
    return false;
  histograms[ histograms.size() - 1 ]->setMaxGradient( dataMaximumGradient );

  return true;
}

void Analyzer2DMaximumGradient::printLine( ofstream& cfgFile,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_MAXIMUMGRADIENT << " " << ( *it )->getMaxGradient() << endl;
}



string Analyzer2DObjects::tagCFG = CFG_TAG_OBJECTS;

bool Analyzer2DObjects::parseLine( KernelConnection *whichKernel,
                                           istringstream& line,
                                           Trace *whichTrace,
                                           vector<Timeline *>& windows,
                                           vector<Histogram *>& histograms )
{
  string strObject;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strObject );
  vector< TObjectOrder > myRows;

  stringstream ss(strObject);

  if ( strObject == "All" )
  {
     Histogram *tmpHisto = histograms[ histograms.size() - 1 ];     
     TObjectOrder totalRows = tmpHisto->getTrace()->getLevelObjects( tmpHisto->getControlWindow()->getLevel() );
     for( TObjectOrder i = 0; i < totalRows; ++i )
     {
       myRows.push_back( i );
     }
  }
  else
  {
    string token;
    char separator = ',';
    while ( getline( ss, token, separator ) )
    {
      TObjectOrder tmpOrder;
      istringstream tmpToken( token );
      if( !( tmpToken >> tmpOrder ) )
        return false;
      myRows.push_back( tmpOrder );
    }
  }

  histograms[ histograms.size() - 1 ]->setSelectedRows( myRows );
  return true;
}

void Analyzer2DObjects::printLine( ofstream& cfgFile,
                                  const vector<Histogram *>::const_iterator it )
{
  vector< TObjectOrder > myRows = ( *it )->getSelectedRows();

  cfgFile << CFG_TAG_OBJECTS << " ";

  TObjectOrder totalRows = ( *it )->getTrace()->getLevelObjects( ( *it )->getControlWindow()->getLevel() );
  if ( myRows.size() == (size_t)totalRows )
  {
    cfgFile << "All";
  }
  else
  {
    size_t i;
    for ( i = 0; i < myRows.size() - 1; ++i )
      cfgFile << myRows[ i ] << ",";

    if ( i == myRows.size() - 1 )
      cfgFile << myRows[ i ];
  }

  cfgFile << endl;
}



string Analyzer2DDrawModeObjects::tagCFG = CFG_TAG_DRAWMODE_OBJECTS;

bool Analyzer2DDrawModeObjects::parseLine( KernelConnection *whichKernel,
                                           istringstream& line,
                                           Trace *whichTrace,
                                           vector<Timeline *>& windows,
                                           vector<Histogram *>& histograms )
{
  string strDrawMode;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strDrawMode );

  if ( drawModeCodes.find( strDrawMode ) != drawModeCodes.end() )
    histograms[ histograms.size() - 1 ]->setDrawModeObjects( drawModeCodes[ strDrawMode ] );

  return true;
}


void Analyzer2DDrawModeObjects::printLine( ofstream& cfgFile,
                                           const vector<Histogram *>::const_iterator it )
{
  cfgFile << CFG_TAG_DRAWMODE_OBJECTS << " " << drawModeTags[ (*it)->getDrawModeObjects() ] << endl;
}


string Analyzer2DDrawModeColumns::tagCFG = CFG_TAG_DRAWMODE_COLUMNS;

bool Analyzer2DDrawModeColumns::parseLine( KernelConnection *whichKernel,
                                           istringstream& line,
                                           Trace *whichTrace,
                                           vector<Timeline *>& windows,
                                           vector<Histogram *>& histograms )
{
  string strDrawMode;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strDrawMode );

  if ( drawModeCodes.find( strDrawMode ) != drawModeCodes.end() )
    histograms[ histograms.size() - 1 ]->setDrawModeColumns( drawModeCodes[ strDrawMode ] );

  return true;
}


void Analyzer2DDrawModeColumns::printLine( ofstream& cfgFile,
                                           const vector<Histogram *>::const_iterator it )
{
  cfgFile << CFG_TAG_DRAWMODE_COLUMNS << " " << drawModeTags[ (*it)->getDrawModeColumns() ] << endl;
}


string Analyzer2DPixelSize::tagCFG = OLDCFG_TAG_AN2D_PIXEL_SIZE;

bool Analyzer2DPixelSize::parseLine( KernelConnection *whichKernel,
                                     istringstream& line,
                                     Trace *whichTrace,
                                     vector<Timeline *>& windows,
                                     vector<Histogram *>& histograms )
{
  string strSize;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strSize );
  istringstream tmpStream( strSize );
  PRV_UINT16 size;

  if ( !( tmpStream >> size ) )
    return false;

  histograms[ histograms.size() - 1 ]->setPixelSize( size );

  return true;
}


void Analyzer2DPixelSize::printLine( ofstream& cfgFile,
                                     const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_PIXEL_SIZE << " " << (*it)->getPixelSize() << endl;
}


string Analyzer2DCodeColor::tagCFG = OLDCFG_TAG_AN2D_CODE_COLOR;
// DEPRECATED
bool Analyzer2DCodeColor::parseLine( KernelConnection *whichKernel, istringstream& line,
                                     Trace *whichTrace,
                                     vector<Timeline *>& windows,
                                     vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCodeColor( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCodeColor( true );
  else
    return false;

  return true;
}
// DEPRECATED
void Analyzer2DCodeColor::printLine( ofstream& cfgFile,
                                     const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_CODE_COLOR << " ";
  if ( ( *it )->getCodeColor() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;

}


string Analyzer2DColorMode::tagCFG = OLDCFG_TAG_AN2D_COLOR_MODE;

bool Analyzer2DColorMode::parseLine( KernelConnection *whichKernel, istringstream& line,
                                     Trace *whichTrace,
                                     vector<Timeline *>& windows,
                                     vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_COLOR_MODE_CODE ) == 0 )
    histograms[ histograms.size() - 1 ]->setColorMode( TColorFunction::COLOR );
  else if ( strBool.compare( OLDCFG_VAL_COLOR_MODE_GRADIENT ) == 0 )
    histograms[ histograms.size() - 1 ]->setColorMode( TColorFunction::GRADIENT );
  else if ( strBool.compare( OLDCFG_VAL_COLOR_MODE_NULL_GRADIENT ) == 0 )
    histograms[ histograms.size() - 1 ]->setColorMode( TColorFunction::NOT_NULL_GRADIENT );
  else
    return false;

  return true;
}

void Analyzer2DColorMode::printLine( ofstream& cfgFile,
                                     const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_COLOR_MODE << " ";
  if ( ( *it )->getColorMode() == TColorFunction::COLOR )
    cfgFile << OLDCFG_VAL_COLOR_MODE_CODE;
  else if( ( *it )->getColorMode() == TColorFunction::GRADIENT )
    cfgFile << OLDCFG_VAL_COLOR_MODE_GRADIENT;
  else if( ( *it )->getColorMode() == TColorFunction::NOT_NULL_GRADIENT )
    cfgFile << OLDCFG_VAL_COLOR_MODE_NULL_GRADIENT;
  cfgFile << endl;

}


string Analyzer2DOnlyTotals::tagCFG = OLDCFG_TAG_AN2D_ONLY_TOTALS;

bool Analyzer2DOnlyTotals::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setOnlyTotals( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setOnlyTotals( true );
  else
    return false;

  return true;
}

void Analyzer2DOnlyTotals::printLine( ofstream& cfgFile,
                                      const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_ONLY_TOTALS << " ";
  if ( ( *it )->getOnlyTotals() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DShortLabels::tagCFG = OLDCFG_TAG_AN2D_SHORT_LABELS;

bool Analyzer2DShortLabels::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setShortLabels( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setShortLabels( true );
  else
    return false;

  return true;
}

void Analyzer2DShortLabels::printLine( ofstream& cfgFile,
                                      const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN2D_SHORT_LABELS << " ";
  if ( ( *it )->getShortLabels() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer2DSynchronize::tagCFG = OLDCFG_TAG_AN2D_SYNCHRONIZE;

bool Analyzer2DSynchronize::parseLine( KernelConnection *whichKernel, istringstream& line,
                                       Trace *whichTrace,
                                       vector<Timeline *>& windows,
                                       vector<Histogram *>& histograms )
{
  string strGroupID;
  TGroupId groupID;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strGroupID );
  istringstream tmpStream( strGroupID );
  if ( !( tmpStream >> groupID ) )
    return false;

  TGroupId realGroupID;
  if( syncRealGroup.find( groupID ) == syncRealGroup.end() )
  {
    realGroupID = SyncWindows::getInstance()->newGroup();
    syncRealGroup[ groupID ] = realGroupID;
  }
  else
    realGroupID = syncRealGroup[ groupID ];

  histograms[ histograms.size() - 1 ]->addToSyncGroup( realGroupID );

  return true;
}

void Analyzer2DSynchronize::printLine( ofstream& cfgFile,
                                       const vector<Histogram *>::const_iterator it )
{
  if( (*it)->isSync() )
  {
    TGroupId realGroupID;
    if( syncRealGroup.find( (*it)->getSyncGroup() ) == syncRealGroup.end() )
    {
      realGroupID = ++lastSyncGroupUsed;
      syncRealGroup[ (*it)->getSyncGroup() ] = realGroupID;
    }
    else
      realGroupID = syncRealGroup[ (*it)->getSyncGroup() ];

    cfgFile << OLDCFG_TAG_AN2D_SYNCHRONIZE << " " << realGroupID << endl;
  }
}

string Analyzer3DControlWindow::tagCFG = OLDCFG_TAG_AN3D_CONTROLWINDOW;

bool Analyzer3DControlWindow::parseLine( KernelConnection *whichKernel, istringstream& line,
    Trace *whichTrace,
    vector<Timeline *>& windows,
    vector<Histogram *>& histograms )
{
  string str3DControlWindow;
  PRV_UINT32 controlWindow;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, str3DControlWindow );
  istringstream tmpValue( str3DControlWindow );
  if ( !( tmpValue >> controlWindow ) )
    return false;

  if ( controlWindow > windows.size() ||
       windows[ controlWindow - 1 ] == nullptr )
    return false;

  if( histograms[ histograms.size() - 1 ]->getControlWindow() != nullptr &&
      !LoadedWindows::validDataWindow( histograms[ histograms.size() - 1 ]->getControlWindow(),
                                       windows[ controlWindow - 1 ] ) )
    return false;

  if( histograms[ histograms.size() - 1 ]->getDataWindow() != nullptr &&
      !LoadedWindows::validDataWindow( histograms[ histograms.size() - 1 ]->getDataWindow(),
                                       windows[ controlWindow - 1 ] ) )
    return false;

  histograms[ histograms.size() - 1 ]->setExtraControlWindow( windows[ controlWindow - 1 ] );

  return true;
}

void Analyzer3DControlWindow::printLine( ofstream& cfgFile,
    const vector<Timeline *>& allWindows,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN3D_CONTROLWINDOW << " ";
  cfgFile << CFGLoader::findWindow( ( *it )->getExtraControlWindow(), allWindows ) + 1;
  cfgFile << endl;
}


string Analyzer3DComputeYScale::tagCFG = OLDCFG_TAG_AN3D_COMPUTEYSCALE;

bool Analyzer3DComputeYScale::parseLine( KernelConnection *whichKernel,
                                         istringstream& line,
                                         Trace *whichTrace,
                                         vector<Timeline *>& windows,
                                         vector<Histogram *>& histograms )
{
  string strBool;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, strBool, ' ' );

  if ( strBool.compare( OLDCFG_VAL_FALSE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCompute3DScale( false );
  else if ( strBool.compare( OLDCFG_VAL_TRUE2 ) == 0 )
    histograms[ histograms.size() - 1 ]->setCompute3DScale( true );
  else
    return false;

  return true;
}

void Analyzer3DComputeYScale::printLine( ofstream& cfgFile,
    const SaveOptions& options,
    const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN3D_COMPUTEYSCALE << " ";
  if ( ( *it )->getCompute3DScale() )
    cfgFile << OLDCFG_VAL_TRUE2;
  else
    cfgFile << OLDCFG_VAL_FALSE2;
  cfgFile << endl;
}


string Analyzer3DMinimum::tagCFG = OLDCFG_TAG_AN3D_MINIMUM;

bool Analyzer3DMinimum::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string str3DMinimum;
  THistogramLimit data3DMinimum;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, str3DMinimum );
  istringstream tmpValue( str3DMinimum );
  if ( !( tmpValue >> data3DMinimum ) )
    return false;
  histograms[ histograms.size() - 1 ]->setExtraControlMin( data3DMinimum );

  return true;
}

void Analyzer3DMinimum::printLine( ofstream& cfgFile,
                                   const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN3D_MINIMUM << " " << ( *it )->getExtraControlMin() << endl;
}


string Analyzer3DMaximum::tagCFG = OLDCFG_TAG_AN3D_MAXIMUM;

bool Analyzer3DMaximum::parseLine( KernelConnection *whichKernel, istringstream& line,
                                   Trace *whichTrace,
                                   vector<Timeline *>& windows,
                                   vector<Histogram *>& histograms )
{
  string str3DMaximum;
  THistogramLimit data3DMaximum;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, str3DMaximum );
  istringstream tmpValue( str3DMaximum );
  if ( !( tmpValue >> data3DMaximum ) )
    return false;
  histograms[ histograms.size() - 1 ]->setExtraControlMax( data3DMaximum );

  return true;
}

void Analyzer3DMaximum::printLine( ofstream& cfgFile,
                                   const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN3D_MAXIMUM << " " << ( *it )->getExtraControlMax() << endl;
}


string Analyzer3DDelta::tagCFG = OLDCFG_TAG_AN3D_DELTA;

bool Analyzer3DDelta::parseLine( KernelConnection *whichKernel, istringstream& line,
                                 Trace *whichTrace,
                                 vector<Timeline *>& windows,
                                 vector<Histogram *>& histograms )
{
  string str3DDelta;
  THistogramLimit data3DDelta;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, str3DDelta );
  istringstream tmpValue( str3DDelta );
  if ( !( tmpValue >> data3DDelta ) )
    return false;
  histograms[ histograms.size() - 1 ]->setExtraControlDelta( data3DDelta );

  return true;
}

void Analyzer3DDelta::printLine( ofstream& cfgFile,
                                 const vector<Histogram *>::const_iterator it )
{
  cfgFile << OLDCFG_TAG_AN3D_DELTA << " " << ( *it )->getExtraControlDelta() << endl;
}


string Analyzer3DFixedValue::tagCFG = OLDCFG_TAG_AN3D_FIXEDVALUE;

bool Analyzer3DFixedValue::parseLine( KernelConnection *whichKernel, istringstream& line,
                                      Trace *whichTrace,
                                      vector<Timeline *>& windows,
                                      vector<Histogram *>& histograms )
{
  string str3DFixedValue;
  double data3DFixedValue;

  if ( windows[ windows.size() - 1 ] == nullptr )
    return false;
  if ( histograms[ histograms.size() - 1 ] == nullptr )
    return false;

  getline( line, str3DFixedValue );
  istringstream tmpValue( str3DFixedValue );
  if ( !( tmpValue >> data3DFixedValue ) )
    return false;
  histograms[ histograms.size() - 1 ]->setPlaneMinValue( data3DFixedValue );

  return true;
}

void Analyzer3DFixedValue::printLine( ofstream& cfgFile,
                                      const vector<Histogram *>::const_iterator it )
{
  double planeMinValue =
    ( *it )->getSelectedPlane() * ( *it )->getExtraControlDelta()
    + ( *it )->getExtraControlMin();
  cfgFile << OLDCFG_TAG_AN3D_FIXEDVALUE << " " << planeMinValue << endl;
}



string TagAliasCFG4D::tagCFG = "";

bool TagAliasCFG4D::parseLine( KernelConnection *whichKernel,
                               istringstream& line,
                               Trace *whichTrace,
                               vector<Timeline *>& windows,
                               vector<Histogram *>& histograms )
{
  string currentCFG4DTag;
  string currentCFG4DAlias;

  getline( line, currentCFG4DTag, '|' );
  getline( line, currentCFG4DAlias );

  if ( isWindowTag )
  {
    // WINDOW/TIMELINE
    if ( windows[ windows.size() - 1 ] == nullptr )
      return false;

    // It has been created
    windows[ windows.size() - 1 ]->setCFG4DAlias( currentCFG4DTag, currentCFG4DAlias );
  }
  else
  {
    // HISTOGRAM
    if ( histograms[ histograms.size() - 1 ] == nullptr )
      return false;

    // It has been created
    histograms[ histograms.size() - 1 ]->setCFG4DAlias( currentCFG4DTag, currentCFG4DAlias );
  }

  return true;
}


void TagAliasCFG4D::printAliasList( ofstream& cfgFile,
                                    const vector<Timeline *>::const_iterator it )
{
  map< string, string > tmpAlias( (*it)->getCFG4DAliasList() );

  for ( map< string, string >::iterator item = tmpAlias.begin(); item != tmpAlias.end(); ++item )
  {
    cfgFile << CFG_TAG_ALIAS_CFG4D << " ";
    cfgFile << item->first << "|" << item->second << endl;
  }
}


void TagAliasCFG4D::printAliasList( ofstream& cfgFile,
                                    const vector<Histogram *>::const_iterator it )
{
  map< string, string > tmpAlias( (*it)->getCFG4DAliasList() );

  for ( map< string, string >::iterator item = tmpAlias.begin(); item != tmpAlias.end(); ++item )
  {
    cfgFile << CFG_TAG_ALIAS_CFG4D << " ";
    cfgFile << item->first << "|" << item->second << endl;
  }
}



string TagLinkCFG4D::tagCFG = CFG_TAG_LINK_CFG4D;

bool TagLinkCFG4D::parseLine( KernelConnection *whichKernel,
                              istringstream& line,
                              Trace *whichTrace,
                              vector<Timeline *>& windows,
                              vector<Histogram *>& histograms )
{
  TCFGS4DGroup indexGroup;
  string originalName;
  string customName;
  string tmpString;
  stringstream tmpStream;
  Timeline *currentWindow = nullptr;
  Histogram *currentHisto = nullptr;

  if( isWindowTag )
  {
    currentWindow = windows[ windows.size() - 1 ];
    if( currentWindow == nullptr ) return false;
  }
  else
  {
    currentHisto = histograms[ histograms.size() - 1 ];
    if( currentHisto == nullptr ) return false;
  }

  getline( line, tmpString, '|' );
  tmpStream.str( tmpString );
  if( !( tmpStream >> indexGroup ) )
    return false;

  // Expected format: [...]|"Top Compose 1|0|Prod.Factor"
  char dummyChar;
  line.get( dummyChar ); // Consume "
  getline( line, originalName, '"' );

  if( currentWindow != nullptr )
    customName = currentWindow->getCFG4DAlias( originalName );
  else
    customName = currentHisto->getCFG4DAlias( originalName );

  CFGS4DGlobalManager::getInstance()->setCustomName( lastGlobalLinkIndex, indexGroup, customName );

  if( currentWindow != nullptr )
  {
    CFGS4DGlobalManager::getInstance()->insertLink( lastGlobalLinkIndex, indexGroup, originalName, currentWindow );
    currentWindow->setCFGS4DGroupLink( originalName, indexGroup );
  }
  else
  {
    CFGS4DGlobalManager::getInstance()->insertLink( lastGlobalLinkIndex, indexGroup, originalName, currentHisto );
    currentHisto->setCFGS4DGroupLink( originalName, indexGroup );
  }
  
  return true;
}

template< typename T >
void TagLinkCFG4D::printLinkList( std::ofstream& cfgFile,
                                  const CFGS4DLinkedPropertiesManager& linkedProperties,
                                  const T *window )
{
  std::set< std::string > linksNames;
  linkedProperties.getLinksName( window, linksNames );
  for( auto it : linksNames )
  {
    cfgFile << CFG_TAG_LINK_CFG4D << " " << linkedProperties.getGroup( window, it )  << "|\"" << it << "\"";
    cfgFile << std::endl;
  }
}


string TagAliasStatisticCFG4D::tagCFG = "";

bool TagAliasStatisticCFG4D::parseLine( KernelConnection *whichKernel,
                                        istringstream& line,
                                        Trace *whichTrace,
                                        vector<Timeline *>& windows,
                                        vector<Histogram *>& histograms )
{
  string currentStatisticCFG4DTag;
  string currentStatisticCFG4DAlias;

  getline( line, currentStatisticCFG4DTag, '|' );
  getline( line, currentStatisticCFG4DAlias );

  if ( !isWindowTag )
  {
    // HISTOGRAM
    if ( histograms[ histograms.size() - 1 ] == nullptr )
      return false;

    // It has been created
    histograms[ histograms.size() - 1 ]->setCFG4DStatisticAlias(
            currentStatisticCFG4DTag, currentStatisticCFG4DAlias );
  }

  return true;
}

void TagAliasStatisticCFG4D::printAliasList( ofstream& cfgFile,
                                             const vector<Histogram *>::const_iterator it )
{
  map< string, string > tmpAlias( (*it)->getCFG4DStatisticsAliasList() );

  for ( map< string, string >::iterator item = tmpAlias.begin(); item != tmpAlias.end(); ++item )
  {
    cfgFile << CFG_TAG_STATISTIC_ALIAS_CFG4D << " ";
    cfgFile << item->first << "|" << item->second << endl;
  }
}

string TagAliasParamCFG4D::tagCFG = "";

bool TagAliasParamCFG4D::parseLine( KernelConnection *whichKernel,
                                    istringstream& line,
                                    Trace *whichTrace,
                                    vector<Timeline *>& windows,
                                    vector<Histogram *>& histograms )
{
  string currentStatisticCFG4DSemanticLevel;
  string currentStatisticCFG4DSemanticFunction;
  string currentStatisticCFG4DSemanticNumParam;
  string currentStatisticCFG4DAlias;

  getline( line, currentStatisticCFG4DSemanticLevel, '|' );
  getline( line, currentStatisticCFG4DSemanticFunction, '|' );
  getline( line, currentStatisticCFG4DSemanticNumParam, '|' );
  getline( line, currentStatisticCFG4DAlias );

  if ( isWindowTag )
  {
    // HISTOGRAM
    if ( windows[ windows.size() - 1 ] == nullptr )
      return false;

    PRV_UINT32 auxNumParam;
    istringstream tmpValue( currentStatisticCFG4DSemanticNumParam );
    if ( !( tmpValue >> auxNumParam ) )
      return false;

    // It has been created
    windows[ windows.size() - 1 ]->setCFG4DParamAlias(
            currentStatisticCFG4DSemanticLevel,
            currentStatisticCFG4DSemanticFunction,
            auxNumParam,
            currentStatisticCFG4DAlias );
  }

  return true;
}


void TagAliasParamCFG4D::printAliasList( ofstream& cfgFile,
                                         const vector<Timeline *>::const_iterator it )
{
  Timeline::TParamAliasKey aliasKey;
  string level;
  string function;
  TParamIndex param;
  string aliasName;

  Timeline::TParamAlias tmpAlias( (*it)->getCFG4DParamAliasList() ); // funcion + num param

  for ( Timeline::TParamAlias::iterator item = tmpAlias.begin(); item != tmpAlias.end(); ++item )
  {
    cfgFile << CFG_TAG_PARAM_ALIAS_CFG4D << " ";

    aliasKey  = (*it)->getCFG4DParamAliasKey( item );
    (*it)->splitCFG4DParamAliasKey( aliasKey, level, function, param );
    aliasName = (*it)->getCFG4DParamAlias( item );

    cfgFile << level << "|" << function << "|" << param << "|" << aliasName << endl;
  }
}

