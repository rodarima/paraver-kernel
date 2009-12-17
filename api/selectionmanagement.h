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

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- *\
 | @file: $HeadURL$
 | @last_commit: $Date$
 | @version:     $Revision$
\* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#ifndef _SELECTION_MANAGEMENT_H
#define _SELECTION_MANAGEMENT_H

#include <vector>
//#include "trace.h"
class Trace;
class HistogramTotals;

template <typename SelType, typename LevelType>
class SelectionManagement
{
  public:
    SelectionManagement();
    ~SelectionManagement();

    void init( Trace *trace );
    void init( HistogramTotals *totals,
               UINT16 idStat,
               THistogramColumn numColumns,
               THistogramColumn whichPlane );

    void copy( const SelectionManagement &selection );

    void setSelected( vector< bool > &selection, LevelType level = (LevelType)0 );
    void setSelected( vector< SelType > &selection, SelType maxElems, LevelType level = (LevelType)0 );

    void getSelected( vector< bool > &selection, LevelType level = (LevelType)0 );
    void getSelected( vector< bool > &selection, SelType first, SelType last, LevelType level = (LevelType)0 );
    void getSelected( vector< SelType > &selection, LevelType level = (LevelType)0 );
    void getSelected( vector< SelType > &selection, SelType first, SelType last, LevelType level = (LevelType)0 );

    bool isSelectedPosition( SelType whichSelected, LevelType level = (LevelType)0 );

  private:
    vector< vector< bool > > selected;
    vector< vector< SelType > > selectedSet;
};


#include "selectionmanagement.cpp"

#endif // _SELECTION_MANAGEMENT_H
