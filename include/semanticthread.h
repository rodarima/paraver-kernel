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


#pragma once


#include "semanticfunction.h"

class SemanticThread : public SemanticFunction
{
  public:
    SemanticThread()
    {}
    ~SemanticThread()
    {}

    virtual bool validRecord( MemoryTrace::iterator *record )
    {
      TRecordType type = record->getType();
      TRecordType mask = getValidateMask();

      if ( type == EMPTYREC )
        return true;

      // Object thread functions
      if( mask == STATE + EVENT && ( type & STATE || type & EVENT ) )
        return true;

      if ( mask & RSEND )
      {
        if ( type & RSEND )
          return true;
        else
          mask -= RSEND;
      }
      else if ( mask & RRECV )
      {
        if ( type & RRECV )
          return true;
        else
          mask -= RRECV;
      }
      return ( ( mask & type ) == mask );
    }

  protected:
    // Valid records for this function
    virtual const TRecordType getValidateMask() = 0;

  private:
};



