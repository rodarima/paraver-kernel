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


#include "noloadtrace.h"
#include "noloadblocks.h"
#include "traceeditblocks.h"
#include "utils/traceparser/processmodel.h"
#include "utils/traceparser/resourcemodel.h"
#include "ktrace.h"

using namespace NoLoad;
using namespace std;

NoLoadTrace::NoLoadTrace( const Trace *whichTrace,
                          MemoryBlocks *whichBlocks,
                          const ProcessModel<>& whichProcessModel,
                          const ResourceModel<>& whichResourceModel )
    : myTrace( whichTrace ), processModel( whichProcessModel ), resourceModel( whichResourceModel )
{
  blocks = dynamic_cast<NoLoadBlocks *>( whichBlocks );
}

NoLoadTrace::~NoLoadTrace()
{}

void NoLoadTrace::insert( MemoryBlocks *blocks )
{
  blocks->resetCountInserted();
}

TTime NoLoadTrace::finish( TTime headerTime, Trace *whichTrace )
{
  return headerTime;
}


MemoryTrace::iterator* NoLoadTrace::empty() const
{
  TRecord *tmpRec;
  PRV_INT64 tmpOffset;
  PRV_UINT16 tmpPos;

  blocks->getEndRecord( &tmpRec, tmpOffset, tmpPos );

  TThreadOrder dummyThread = 0;
  MemoryTrace::iterator *it = new NoLoadTrace::iterator( myTrace, blocks, dummyThread, tmpRec, tmpOffset, tmpPos );
  //it->setType( EMPTYREC );

  return it;
}


MemoryTrace::iterator* NoLoadTrace::begin() const
{
  TRecord *tmpRec;
  PRV_INT64 tmpOffset;
  PRV_UINT16 tmpPos;

  blocks->getBeginRecord( &tmpRec, tmpOffset, tmpPos );

  TThreadOrder dummyThread = 0;
  return new iterator( myTrace, blocks, dummyThread, tmpRec, tmpOffset, tmpPos );
}


MemoryTrace::iterator* NoLoadTrace::end() const
{
  TRecord *tmpRec;
  PRV_INT64 tmpOffset;
  PRV_UINT16 tmpPos;

  blocks->getEndRecord( &tmpRec, tmpOffset, tmpPos );

  TThreadOrder dummyThread = 0;
  return new iterator( myTrace, blocks, dummyThread, tmpRec, tmpOffset, tmpPos );
}


MemoryTrace::iterator* NoLoadTrace::threadBegin( TThreadOrder whichThread ) const
{
  TRecord *tmpRec;
  PRV_INT64 tmpOffset;
  PRV_UINT16 tmpPos;

  blocks->getBeginThreadRecord( whichThread, &tmpRec, tmpOffset, tmpPos );

  return new ThreadIterator( myTrace, blocks, whichThread, tmpRec, tmpOffset, tmpPos );
}

MemoryTrace::iterator* NoLoadTrace::threadEnd( TThreadOrder whichThread ) const
{
  TRecord *tmpRec;
  PRV_INT64 tmpOffset;
  PRV_UINT16 tmpPos;

  blocks->getEndThreadRecord( whichThread, &tmpRec, tmpOffset, tmpPos );

  return new ThreadIterator( myTrace, blocks, whichThread, tmpRec, tmpOffset, tmpPos );
}

MemoryTrace::iterator* NoLoadTrace::CPUBegin( TCPUOrder whichCPU ) const
{
  vector<TRecord *> records;
  vector<PRV_INT64> offsets;
  vector<PRV_UINT16> pos;
  vector<TThreadOrder> threads;
  TNodeOrder tmpNode;
  TCPUOrder tmpCPU;

  resourceModel.getCPULocation( whichCPU, tmpNode, tmpCPU );
  processModel.getThreadsPerNode( tmpNode + 1, threads );

  for ( TThreadOrder i = 0; i < threads.size(); ++i )
  {
    TRecord *tmpRec;
    PRV_INT64 tmpOffset;
    PRV_UINT16 tmpPos;

    blocks->getBeginThreadRecord( threads[ i ], &tmpRec, tmpOffset, tmpPos );
    records.push_back( tmpRec );
    offsets.push_back( tmpOffset );
    pos.push_back( tmpPos );
  }

  return new CPUIterator( myTrace, blocks, whichCPU, threads, records, offsets, pos );
}

MemoryTrace::iterator* NoLoadTrace::CPUEnd( TCPUOrder whichCPU ) const
{
  vector<TRecord *> records;
  vector<PRV_INT64> offsets;
  vector<PRV_UINT16> pos;
  vector<TThreadOrder> threads;
  TNodeOrder tmpNode;
  TCPUOrder tmpCPU;

  resourceModel.getCPULocation( whichCPU, tmpNode, tmpCPU );
  processModel.getThreadsPerNode( tmpNode + 1, threads );

  for ( TThreadOrder i = 0; i < threads.size(); ++i )
  {
    TRecord *tmpRec;
    PRV_INT64 tmpOffset;
    PRV_UINT16 tmpPos;

    blocks->getEndThreadRecord( threads[ i ], &tmpRec, tmpOffset, tmpPos );
    records.push_back( tmpRec );
    offsets.push_back( tmpOffset );
    pos.push_back( tmpPos );
  }
  return new CPUIterator( myTrace, blocks, whichCPU, threads, records, offsets, pos, true );
}

void NoLoadTrace::getRecordByTimeThread( vector<MemoryTrace::iterator *>& listIter,
    TRecordTime whichTime ) const
{
  for ( TThreadOrder ii = 0; ii < processModel.totalThreads(); ++ii )
  {
    if ( listIter[ ii ] != nullptr )
    {
      delete listIter[ ii ];
      listIter[ ii ] = nullptr;
    }
  }

  for ( TThreadOrder iThread = 0; iThread < processModel.totalThreads(); ++iThread )
  {
    ThreadIterator *it;
    TRecord *tmpRec;
    PRV_INT64 tmpOffset;
    PRV_UINT16 tmpPos;

    blocks->getThreadRecordByTime( iThread, whichTime, &tmpRec, tmpOffset, tmpPos );
    if( tmpRec == nullptr )
      it = ( NoLoadTrace::ThreadIterator * )threadEnd( iThread );
    else
      it = new ThreadIterator( myTrace, blocks, iThread, tmpRec, tmpOffset, tmpPos );
    while ( !it->isNull() && it->getTime() > whichTime )
      --( *it );
    if ( it->isNull() )
    {
      delete it;
      it = ( NoLoadTrace::ThreadIterator * )threadBegin( iThread );
    }

    listIter[ iThread ] = it;
  }
}

void NoLoadTrace::getRecordByTimeCPU( vector<MemoryTrace::iterator *>& listIter,
                                      TRecordTime whichTime ) const
{
  for ( TCPUOrder ii = 0; ii < resourceModel.totalCPUs(); ++ii )
  {
    if ( listIter[ ii ] != nullptr )
    {
      delete listIter[ ii ];
      listIter[ ii ] = nullptr;
    }
  }

  for ( TCPUOrder ii = 0; ii < resourceModel.totalCPUs(); ++ii )
  {
    vector<TRecord *> records;
    vector<PRV_INT64> offsets;
    vector<PRV_UINT16> pos;
    vector<TThreadOrder> threads;
    TNodeOrder tmpNode;
    TCPUOrder tmpCPU;

    resourceModel.getCPULocation( ii, tmpNode, tmpCPU );
    processModel.getThreadsPerNode( tmpNode + 1, threads );

    for ( TThreadOrder iThread = 0; iThread < threads.size(); ++iThread )
    {
      TRecord *tmpRec;
      PRV_INT64 tmpOffset;
      PRV_UINT16 tmpPos;

      blocks->getThreadRecordByTime( threads[ iThread ], whichTime, &tmpRec, tmpOffset, tmpPos );
      if ( tmpRec == nullptr )
      {
        blocks->getEndThreadRecord( threads[ iThread ], &tmpRec, tmpOffset, tmpPos );
        while( tmpRec->time > whichTime )
          blocks->getPrevRecord( threads[ iThread ], &tmpRec, tmpOffset, tmpPos );
      }
      records.push_back( tmpRec );
      offsets.push_back( tmpOffset );
      pos.push_back( tmpPos );
    }

    CPUIterator *tmpIt = new CPUIterator( myTrace, blocks, ii, threads, records, offsets, pos );

    while ( !tmpIt->isNull() && tmpIt->getTime() > whichTime )
      --( *tmpIt );
    if ( tmpIt->isNull() )
    {
      delete tmpIt;
      tmpIt = ( NoLoadTrace::CPUIterator * )CPUBegin( ii );
    }
    listIter[ ii ] = tmpIt;

  }
}


NoLoadTrace::iterator::iterator( const Trace *whichTrace, NoLoadBlocks *whichBlocks )
    : MemoryTrace::iterator( whichTrace ), blocks( whichBlocks ), destroyed( false )
{}


NoLoadTrace::iterator::iterator( const Trace *whichTrace,
                                 NoLoadBlocks *whichBlocks,
                                 TThreadOrder whichThread,
                                 TRecord *whichRecord,
                                 PRV_INT64 whichOffset,
                                 PRV_INT16 whichPos )
    : MemoryTrace::iterator( whichTrace ), blocks( whichBlocks ), thread( whichThread ),
    offset( whichOffset ), recPos( whichPos ), destroyed( false )
{
  record = whichRecord;
}

NoLoadTrace::iterator::~iterator()
{
  if ( !destroyed )
  {
    if ( !( record == nullptr || offset == -1 ) )
      blocks->decNumUseds( offset );

    destroyed = true;
  }
}

inline void NoLoadTrace::iterator::operator++()
{
  blocks->getNextRecord( ( TRecord ** )&record, offset, recPos );
}

inline void NoLoadTrace::iterator::operator--()
{
  blocks->getPrevRecord( ( TRecord ** )&record, offset, recPos );
}

inline MemoryTrace::iterator& NoLoadTrace::iterator::operator=( const MemoryTrace::iterator& copy )
{
  if ( this != &copy )
  {
    NoLoadTrace::iterator *tmpCopy = ( NoLoadTrace::iterator * ) & copy;
    blocks->decNumUseds( offset );
    thread = tmpCopy->thread;
    offset = tmpCopy->offset;
    recPos = tmpCopy->recPos;
    record = tmpCopy->getRecord();
    blocks->incNumUseds( offset );
  }
  return *this;
}

inline NoLoadTrace::iterator *NoLoadTrace::iterator::clone() const
{
  if ( offset != -1 )
    blocks->incNumUseds( offset );

  return new NoLoadTrace::iterator( *this );
}

inline TRecordType NoLoadTrace::iterator::getType() const
{
  return ( ( TRecord * )record )->type;
}

inline TRecordTime NoLoadTrace::iterator::getTime() const
{
  return ( ( TRecord * )record )->time;
}

inline TThreadOrder NoLoadTrace::iterator::getThread() const
{
  if( !( ( ( TRecord * )record )->type & EMPTYREC ) )
  {
    if( ( ( TRecord * )record )->type & COMM )
    {
      if( ( ( TRecord * )record )->type & SEND )
        return blocks->getSenderThread( ( ( TRecord * )record )->URecordInfo.commRecord.index );
      else
        return blocks->getReceiverThread( ( ( TRecord * )record )->URecordInfo.commRecord.index );
    }
  }

  return ( ( TRecord * )record )->thread;
}

inline TCPUOrder NoLoadTrace::iterator::getCPU() const
{
  return ( ( TRecord * )record )->CPU;
}

inline TObjectOrder NoLoadTrace::iterator::getOrder() const
{
  if( ( ( TRecord * )record )->type & COMM )
  {
    if( ( ( TRecord * )record )->type & SEND )
      return blocks->getSenderThread( ( ( TRecord * )record )->URecordInfo.commRecord.index );
    else
      return blocks->getReceiverThread( ( ( TRecord * )record )->URecordInfo.commRecord.index );
  }

  return thread;
}

inline TEventType NoLoadTrace::iterator::getEventType() const
{
  return ( ( TRecord * )record )->URecordInfo.eventRecord.type;
}

inline TSemanticValue NoLoadTrace::iterator::getEventValue() const
{
  double tmpPrecision = myTrace->getEventTypePrecision( ( ( TRecord * )record )->URecordInfo.eventRecord.type );
  if( tmpPrecision != 0.0 )
    return ( ( TRecord * )record )->URecordInfo.eventRecord.value * tmpPrecision;
  return ( ( TRecord * )record )->URecordInfo.eventRecord.value;
}

inline TEventValue    NoLoadTrace::iterator::getEventValueAsIs() const
{
    return ( ( TRecord * )record )->URecordInfo.eventRecord.value;
}

inline TState NoLoadTrace::iterator::getState() const
{
  return ( ( TRecord * )record )->URecordInfo.stateRecord.state;
}

inline TRecordTime NoLoadTrace::iterator::getStateEndTime() const
{
  return ( ( TRecord * )record )->URecordInfo.stateRecord.endTime;
}

inline TCommID NoLoadTrace::iterator::getCommIndex() const
{
  return ( ( TRecord * )record )->URecordInfo.commRecord.index;
}

inline void NoLoadTrace::iterator::setTime( const TRecordTime whichTime )
{
  ( ( TRecord * )record )->time = whichTime;
}

inline void NoLoadTrace::iterator::setType( const TRecordType whichType )
{
  ( ( TRecord * )record )->type = whichType;
}

inline void NoLoadTrace::iterator::setStateEndTime( const TRecordTime whichEndTime )
{
  ( ( TRecord * )record )->URecordInfo.stateRecord.endTime = whichEndTime;
}


NoLoadTrace::ThreadIterator::ThreadIterator( const Trace *whichTrace,
                                             NoLoadBlocks *whichBlocks,
                                             TThreadOrder whichThread,
                                             TRecord *whichRecord,
                                             PRV_INT64 whichOffset,
                                             PRV_INT16 whichPos )
    : NoLoadTrace::iterator( whichTrace, whichBlocks, whichThread, whichRecord, whichOffset, whichPos )
{
  record = whichRecord;
}

NoLoadTrace::ThreadIterator::~ThreadIterator()
{
}

inline TThreadOrder NoLoadTrace::ThreadIterator::getThread() const
{
  return thread;
}

inline TObjectOrder NoLoadTrace::ThreadIterator::getOrder() const
{
  return thread;
}

inline void NoLoadTrace::ThreadIterator::operator++()
{
  blocks->getNextRecord( thread, ( TRecord ** )&record, offset, recPos );
}

inline void NoLoadTrace::ThreadIterator::operator--()
{
  blocks->getPrevRecord( thread, ( TRecord ** )&record, offset, recPos );
}

inline MemoryTrace::iterator& NoLoadTrace::ThreadIterator::operator=( const MemoryTrace::iterator & copy )
{
  if ( this != &copy )
  {
    NoLoadTrace::ThreadIterator *tmpCopy = ( NoLoadTrace::ThreadIterator * ) & copy;
    blocks->decNumUseds( offset );
    thread = tmpCopy->thread;
    offset = tmpCopy->offset;
    recPos = tmpCopy->recPos;
    record = tmpCopy->getRecord();
    blocks->incNumUseds( offset );
  }
  return *this;
}

inline NoLoadTrace::ThreadIterator *NoLoadTrace::ThreadIterator::clone() const
{
  if ( offset != -1 )
    blocks->incNumUseds( offset );

  return new NoLoadTrace::ThreadIterator( *this );
}

NoLoadTrace::CPUIterator::CPUIterator( const Trace *whichTrace, 
                                       NoLoadBlocks *whichBlocks,
                                       TCPUOrder whichCPU,
                                       vector<TThreadOrder>& whichThreads,
                                       vector<TRecord *>& whichRecords,
                                       vector<PRV_INT64>& whichOffsets,
                                       vector<PRV_UINT16>& whichPos,
                                       bool notMove )
    : NoLoadTrace::iterator( whichTrace, whichBlocks ), cpu( whichCPU ), threads( whichThreads ),
    threadRecords( whichRecords ), offset( whichOffsets ), recPos( whichPos )
{
  if( notMove )
    lastThread = 0;
  else
    lastThread = minThread();
  record = threadRecords[ lastThread ];
}

NoLoadTrace::CPUIterator::~CPUIterator()
{
  if( !destroyed )
  {
    for ( TThreadOrder i = 0; i < threads.size(); ++i )
    {
      if ( !( threadRecords[ i ] == nullptr || offset[ i ] == -1 ) )
        blocks->decNumUseds( offset[ i ] );
    }
    destroyed = true;
  }
}

inline TThreadOrder NoLoadTrace::CPUIterator::getThread() const
{
  return lastThread;
}

inline TObjectOrder NoLoadTrace::CPUIterator::getOrder() const
{
  return cpu;
}

inline void NoLoadTrace::CPUIterator::operator++()
{
  TThreadOrder inLastPos = 0;
  blocks->getNextRecord( threads[ lastThread ], &threadRecords[ lastThread ],
                         offset[ lastThread ], recPos[ lastThread ] );
  for ( TThreadOrder iThread = 0; iThread < threads.size(); ++iThread )
  {
    if ( threadRecords[ iThread ] == nullptr )
      ++inLastPos;
  }
  if ( inLastPos == threads.size() )
  {
    record = nullptr;
    return;
  }

  lastThread = minThread();
  record = threadRecords[ lastThread ];
}

inline void NoLoadTrace::CPUIterator::operator--()
{
  TThreadOrder inFirstPos = 0;
  blocks->getPrevRecord( threads[ lastThread ], &threadRecords[ lastThread ],
                         offset[ lastThread ], recPos[ lastThread ] );
  for ( TThreadOrder iThread = 0; iThread < threads.size(); ++iThread )
  {
    if ( threadRecords[ iThread ] == nullptr  )
      ++inFirstPos;
  }
  if ( inFirstPos == threads.size() )
  {
    record = nullptr;
    return;
  }

  lastThread = maxThread();
  record = threadRecords[ lastThread ];
}

inline MemoryTrace::iterator& NoLoadTrace::CPUIterator::operator=( const MemoryTrace::iterator & copy )
{
  if ( this != &copy )
  {
    NoLoadTrace::CPUIterator *tmpCopy = ( NoLoadTrace::CPUIterator * ) & copy;
    blocks->decNumUseds( offset[ lastThread ] );
    cpu = tmpCopy->cpu;
    threads = tmpCopy->threads;
    threadRecords = tmpCopy->threadRecords;
    offset = tmpCopy->offset;
    recPos = tmpCopy->recPos;
    lastThread = tmpCopy->lastThread;
    record = tmpCopy->getRecord();
    blocks->incNumUseds( offset[ lastThread ] );
  }
  return *this;
}

inline NoLoadTrace::CPUIterator *NoLoadTrace::CPUIterator::clone() const
{
  for ( TThreadOrder i = 0; i < threads.size(); ++i )
    if ( offset[ i ] != -1 && threadRecords[ i ] != nullptr )
      blocks->incNumUseds( offset[ i ] );

  return new NoLoadTrace::CPUIterator( *this );
}

inline TThreadOrder NoLoadTrace::CPUIterator::minThread()
{
  map<TRecord *, TThreadOrder, NoLoad::ltrecord> sortedRecords;

  setToMyCPUForward();
  for ( TThreadOrder iThread = 0; iThread < threads.size(); ++iThread )
  {
    if ( !( threadRecords[ iThread ] == nullptr )/*|| offset[ iThread ] == -1 )*/ )
      sortedRecords[ threadRecords[ iThread ] ] = iThread;
  }

  if ( sortedRecords.begin() == sortedRecords.end() )
    return 0;

  return sortedRecords.begin()->second;
}

inline TThreadOrder NoLoadTrace::CPUIterator::maxThread()
{
  map<TRecord *, TThreadOrder, NoLoad::ltrecord> sortedRecords;

  setToMyCPUBackward();
  for ( TThreadOrder iThread = 0; iThread < threads.size(); ++iThread )
  {
    if ( !( threadRecords[ iThread ] == nullptr )/*|| offset[ iThread ] == -1 )*/ )
      sortedRecords[ threadRecords[ iThread ] ] = iThread;
  }

  if ( sortedRecords.begin() == sortedRecords.end() )
    return 0;

  map<TRecord *, TThreadOrder, ltrecord>::iterator it = sortedRecords.end();
  --it;
  return it->second;
}

inline void NoLoadTrace::CPUIterator::setToMyCPUForward()
{
  for ( TThreadOrder iThread = 0; iThread < threads.size(); ++iThread )
  {
    while ( !( threadRecords[ iThread ] == nullptr )/*|| offset[ iThread ] == -1 )*/ && threadRecords[ iThread ]->CPU != cpu )
    {
      blocks->getNextRecord( threads[ iThread ], &threadRecords[ iThread ],
                             offset[ iThread ], recPos[ iThread ] );
    }
  }
}

inline void NoLoadTrace::CPUIterator::setToMyCPUBackward()
{
  for ( TThreadOrder iThread = 0; iThread < threads.size(); ++iThread )
  {
    while ( !( threadRecords[ iThread ] == nullptr )/*|| offset[ iThread ] == -1 )*/ &&  threadRecords[ iThread ]->CPU != cpu )
    {
      blocks->getPrevRecord( threads[ iThread ], &threadRecords[ iThread ],
                             offset[ iThread ], recPos[ iThread ] );
    }
  }
}
