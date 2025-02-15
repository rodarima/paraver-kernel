/*****************************************************************************\
 *                        ANALYSIS PERFORMANCE TOOLS                         *
 *                               libparaver-api                              *
 *                       Paraver Main Computing Library                      *
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


#include "semanticderivedfunctions.h"
#include "kwindow.h"

using namespace std;

string DerivedAdd::name = "add";
TSemanticValue DerivedAdd::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] + myInfo->values[ 1 ];

  return tmp;
}


string DerivedProduct::name = "product";
TSemanticValue DerivedProduct::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] * myInfo->values[ 1 ];

  return tmp;
}


string DerivedSubstract::name = "substract";
TSemanticValue DerivedSubstract::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] - myInfo->values[ 1 ];

  return tmp;
}


string DerivedDivide::name = "divide";
TSemanticValue DerivedDivide::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0.0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  if( myInfo->values[ 1 ] == 0 )
    return 0.0;

  tmp = myInfo->values[ 0 ] / myInfo->values[ 1 ];

  return tmp;
}


string DerivedMaximum::name = "maximum";
TSemanticValue DerivedMaximum::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] > myInfo->values[ 1 ] ?
        myInfo->values[ 0 ] :
        myInfo->values[ 1 ];

  return tmp;
}


string DerivedMinimum::name = "minimum";
TSemanticValue DerivedMinimum::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] < myInfo->values[ 1 ] ?
        myInfo->values[ 0 ] :
        myInfo->values[ 1 ];

  return tmp;
}


string DerivedDifferent::name = "different";
TSemanticValue DerivedDifferent::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] != myInfo->values[ 1 ] ?
        1 :
        0;

  return tmp;
}


string ControlDerivedClearBy::name = "controlled: clear by";
TSemanticValue ControlDerivedClearBy::execute( const SemanticInfo *info )
{
  static const int controlWindow = 1;
  static const int dataWindow = 0;
  TSemanticValue result = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;
  TObjectOrder tmpOrder = myInfo->callingInterval->getOrder();

  if( myInfo->values[ controlWindow ] < lastControlValue[ tmpOrder ] )
  {
    result = 0;
    lastDataBeginTime[ tmpOrder ] = myInfo->dataBeginTime;
  }
  else
  {
    if( lastDataBeginTime[ tmpOrder ] != myInfo->dataBeginTime || myInfo->dataBeginTime == 0.0 )
      result = myInfo->values[ dataWindow ];
  }

  lastControlValue[ tmpOrder ] = myInfo->values[ controlWindow ];

  return result;
}

void ControlDerivedClearBy::init( KTimeline *whichWindow )
{
  TObjectOrder size = 0;

  lastControlValue.clear();
  lastDataBeginTime.clear();

  if( whichWindow->getLevel() >= TTraceLevel::SYSTEM )
    size = whichWindow->getTrace()->totalCPUs();
  else
    size = whichWindow->getTrace()->totalThreads();

  lastControlValue.reserve( size );
  lastDataBeginTime.reserve( size );
  for( TObjectOrder i = 0; i < size; i++ )
  {
    lastControlValue.push_back( 0 );
    lastDataBeginTime.push_back( 0.0 );
  }
}


string ControlDerivedMaximum::name = "controlled: maximum";
TSemanticValue ControlDerivedMaximum::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] > myInfo->values[ 1 ] ?
        myInfo->values[ 0 ] :
        myInfo->values[ 1 ];

  return tmp;
}


string ControlDerivedAdd::name = "controlled: add";
TSemanticValue ControlDerivedAdd::execute( const SemanticInfo *info )
{
  TSemanticValue tmp = 0;
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;

  tmp = myInfo->values[ 0 ] + myInfo->values[ 1 ];

  return tmp;
}


string ControlDerivedEnumerate::name = "controlled: enumerate";
TSemanticValue ControlDerivedEnumerate::execute( const SemanticInfo *info )
{
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;
  TObjectOrder tmpOrder = myInfo->callingInterval->getOrder();

  if( myInfo->values[ 1 ] < prevControlValue[ tmpOrder ] )
    myEnumerate[ tmpOrder ] = 0;
  else if( prevDataTime[ tmpOrder ] == 0 || myInfo->dataBeginTime != prevDataTime[ tmpOrder ] && myInfo->values[ 0 ] != 0.0 )
    ++myEnumerate[ tmpOrder ];

  prevControlValue[ tmpOrder ] = myInfo->values[ 1 ];
  prevDataTime[ tmpOrder ] = myInfo->dataBeginTime;

  return myEnumerate[ tmpOrder ];
}


void ControlDerivedEnumerate::init( KTimeline *whichWindow )
{
  TObjectOrder size = 0;

  myEnumerate.clear();
  prevControlValue.clear();
  prevDataTime.clear();

  if( whichWindow->getLevel() >= TTraceLevel::SYSTEM )
    size = whichWindow->getTrace()->totalCPUs();
  else
    size = whichWindow->getTrace()->totalThreads();

  myEnumerate.reserve( size );
  prevControlValue.reserve( size );
  prevDataTime.reserve( size );

  for( TObjectOrder i = 0; i < size; i++ )
  {
    myEnumerate.push_back( 0 );
    prevControlValue.push_back( 0 );
    prevDataTime.push_back( 0 );
  }
}


string ControlDerivedAverage::name = "controlled: average";
TSemanticValue ControlDerivedAverage::execute( const SemanticInfo *info )
{
  const SemanticHighInfo *myInfo = ( const SemanticHighInfo * ) info;
  TObjectOrder tmpOrder = myInfo->callingInterval->getOrder();

  if( myInfo->newControlBurst )
  {
    totalValue[ tmpOrder ] = 0.0;
    totalTime[ tmpOrder ] = 0.0;
  }

  TRecordTime tmpBurstDuration = myInfo->dataEndTime - myInfo->dataBeginTime;
  totalValue[ tmpOrder ] += myInfo->values[ 1 ] * tmpBurstDuration;
  totalTime[ tmpOrder ] += tmpBurstDuration;

  if ( totalTime[ tmpOrder ] == 0.0 )
    return 0.0;

  return totalValue[ tmpOrder ] / totalTime[ tmpOrder ];
}

void ControlDerivedAverage::init( KTimeline *whichWindow )
{
  TObjectOrder size = 0;

  totalValue.clear();
  totalTime.clear();

  if( whichWindow->getLevel() >= TTraceLevel::SYSTEM )
    size = whichWindow->getTrace()->totalCPUs();
  else
    size = whichWindow->getTrace()->totalThreads();

  totalValue.reserve( size );
  totalTime.reserve( size );

  for( TObjectOrder i = 0; i < size; ++i )
  {
    totalValue.push_back( 0.0 );
    totalTime.push_back( 0.0 );
  }
}
