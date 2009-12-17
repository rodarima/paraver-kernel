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

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- *\
 | @file: $HeadURL$
 | @last_commit: $Date$
 | @version:     $Revision$
\* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#ifndef TRACEBODYIO_V1_H_INCLUDED
#define TRACEBODYIO_V1_H_INCLUDED

#include "tracebodyio.h"
#include "tracestream.h"

using namespace std;

// Paraver trace old format file
class TraceBodyIO_v1 : public TraceBodyIO
{
  public:
    static const UINT8 StateRecord = '1';
    static const UINT8 EventRecord = '2';
    static const UINT8 CommRecord = '3';
    static const UINT8 GlobalCommRecord = '4';

    bool ordered() const;
    void read( TraceStream *file, MemoryBlocks& records,
               hash_set<TEventType>& events ) const;
    void write( fstream& whichStream,
                const KTrace& whichTrace,
                MemoryTrace::iterator *record ) const;
    void writeCommInfo( fstream& whichStream,
                        const KTrace& whichTrace ) const;
    void writeEvents( fstream& whichStream,
                      const KTrace& whichTrace,
                      vector<MemoryTrace::iterator *>& recordList ) const;
  protected:

  private:
    void readState( const string& line, MemoryBlocks& records ) const;
    void readEvent( const string& line, MemoryBlocks& records,
                    hash_set<TEventType>& events ) const;
    void readComm( const string& line, MemoryBlocks& records ) const;
    void readGlobalComm( const string& line, MemoryBlocks& records ) const;
    bool readCommon( istringstream& line,
                     TCPUOrder& CPU,
                     TApplOrder& appl,
                     TTaskOrder& task,
                     TThreadOrder& thread,
                     TRecordTime& time ) const;

    bool writeState( string& line,
                     const KTrace& whichTrace,
                     const MemoryTrace::iterator *record ) const;
    bool writeEvent( string& line,
                     const KTrace& whichTrace,
                     const MemoryTrace::iterator *record,
                     bool needCommons = true ) const;
    bool writeComm( string& line,
                    const KTrace& whichTrace,
                    const MemoryTrace::iterator *record ) const;
    bool writeGlobalComm( string& line,
                          const KTrace& whichTrace,
                          const MemoryTrace::iterator *record ) const;
    void writeCommon( ostringstream& line,
                      const KTrace& whichTrace,
                      const MemoryTrace::iterator *record ) const;

};


#endif // TRACEBODYIO_V1_H_INCLUDED
