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


#include "trace.h"
#include "plaintrace.h"
#include "plainblocks.h"
#include "utils/traceparser/processmodel.h"
#include "utils/traceparser/resourcemodel.h"
#include <iostream>

using namespace Plain;
using namespace std;

PlainTrace::PlainTrace( const Trace *whichTrace,
                        const ProcessModel<>& whichProcessModel,
                        const ResourceModel<>& whichResourceModel ):
    myTrace( whichTrace ), processModel( whichProcessModel ), resourceModel( whichResourceModel )

{
  numThreads = processModel.totalThreads();
  numCPUs = resourceModel.totalCPUs();

  for ( TThreadOrder i = 0; i < numThreads; ++i )
    traceIndex.push_back( Index<pair<PRV_UINT32, PRV_UINT32> >( 1000 ) );
}


void PlainTrace::insert( MemoryBlocks *blocks )
{
  myBlocks = dynamic_cast<PlainBlocks *>( blocks );

  for ( PRV_UINT16 i = 0; i < blocks->getCountInserted(); i++ )
  {
    TLastRecord *tmpLast = ( TLastRecord * )blocks->getLastRecord( i );
    if ( traceIndex.size() < tmpLast->thread + 1 )
      traceIndex.push_back( Index<pair<PRV_UINT32, PRV_UINT32> >( 1000 ) );
    
    traceIndex[ tmpLast->thread ].indexRecord( tmpLast->time,
                                               pair<PRV_UINT32, PRV_UINT32>( tmpLast->block, tmpLast->pos )
                                             );
  }

  blocks->resetCountInserted();
}


TTime PlainTrace::finish( TTime headerTime, Trace *whichTrace )
{
  return headerTime;
}

MemoryTrace::iterator* PlainTrace::empty() const
{
  PlainBlocks *dummyBlocks = new PlainBlocks( *myBlocks ); // TODO CHANGE THIS
  dummyBlocks->newRecord();
  dummyBlocks->setType( EMPTYREC );

  return new PlainTrace::iterator( dummyBlocks, myTrace );
}

MemoryTrace::iterator* PlainTrace::begin() const
{
  return new PlainTrace::iterator( );
}

MemoryTrace::iterator* PlainTrace::end() const
{
  return new PlainTrace::iterator( );
}

MemoryTrace::iterator* PlainTrace::threadBegin( TThreadOrder whichThread ) const
{
  return new PlainTrace::ThreadIterator( myBlocks, myTrace, 0, 0, whichThread );
}

MemoryTrace::iterator* PlainTrace::threadEnd( TThreadOrder whichThread ) const
{
  PRV_UINT32 block = myBlocks->blocks[ whichThread ].size() - 1;
  PRV_UINT32 pos = myBlocks->currentRecord[ whichThread ];
  return new PlainTrace::ThreadIterator( myBlocks, myTrace, block, pos, whichThread );
}

MemoryTrace::iterator* PlainTrace::CPUBegin( TCPUOrder whichCPU ) const
{
  vector<PRV_UINT32> block;
  vector<PRV_UINT32> pos;
  TThreadOrder numThreads;
  vector<TThreadOrder> threads;
  TNodeOrder tmpNode;
  TCPUOrder tmpCPU;

  resourceModel.getCPULocation( whichCPU, tmpNode, tmpCPU );
  processModel.getThreadsPerNode( tmpNode + 1, threads );

  numThreads = threads.size();
  block.insert( block.begin(), numThreads, 0 );
  pos.insert( pos.begin(), numThreads, 0 );

  return new PlainTrace::CPUIterator( myBlocks, myTrace, block, pos, numThreads, threads, whichCPU );
}

MemoryTrace::iterator* PlainTrace::CPUEnd( TCPUOrder whichCPU ) const
{
  vector<PRV_UINT32> block;
  vector<PRV_UINT32> pos;
  TThreadOrder numThreads;
  vector<TThreadOrder> threads;
  TNodeOrder tmpNode;
  TCPUOrder tmpCPU;

  resourceModel.getCPULocation( whichCPU, tmpNode, tmpCPU );
  processModel.getThreadsPerNode( tmpNode + 1, threads );

  numThreads = threads.size();
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    block.push_back( myBlocks->blocks[ iThread ].size() - 1 );
    pos.push_back( myBlocks->currentRecord[ iThread ] );
  }

  return new PlainTrace::CPUIterator( myBlocks, myTrace, block, pos, numThreads, threads, whichCPU );
}

void PlainTrace::getRecordByTimeThread( vector<MemoryTrace::iterator *>& listIter,
                                        TRecordTime whichTime ) const
{
  for ( TThreadOrder ii = 0; ii < numThreads; ++ii )
  {
    if ( listIter[ ii ] != nullptr )
    {
      delete listIter[ ii ];
      listIter[ ii ] = nullptr;
    }
  }

  for ( TThreadOrder ii = 0; ii < numThreads; ++ii )
  {
    pair<PRV_UINT32, PRV_UINT32> blockPos;
    ThreadIterator *tmpIt;
    if ( traceIndex[ ii ].findRecord( whichTime, blockPos ) )
    {
      tmpIt = new ThreadIterator( myBlocks, myTrace, blockPos.first, blockPos.second, ii );
      while ( !tmpIt->isNull() && tmpIt->getTime() > whichTime )
        --( *tmpIt );
      if ( tmpIt->isNull() )
      {
        delete tmpIt;
        tmpIt = ( PlainTrace::ThreadIterator * )threadBegin( ii );
      }
    }
    else
      tmpIt = ( PlainTrace::ThreadIterator * )threadBegin( ii );
    listIter[ ii ] = tmpIt;
  }
}

void PlainTrace::getRecordByTimeCPU( vector<MemoryTrace::iterator *>& listIter,
                                     TRecordTime whichTime ) const
{
  for ( TCPUOrder ii = 0; ii < numCPUs; ++ii )
  {
    if ( listIter[ ii ] != nullptr )
    {
      delete listIter[ ii ];
      listIter[ ii ] = nullptr;
    }
  }

  for ( TCPUOrder ii = 0; ii < numCPUs; ++ii )
  {
    vector<PRV_UINT32> block;
    vector<PRV_UINT32> pos;
    TThreadOrder numThreads;
    vector<TThreadOrder> threads;
    TNodeOrder tmpNode;
    TCPUOrder tmpCPU;

    resourceModel.getCPULocation( ii, tmpNode, tmpCPU );
    processModel.getThreadsPerNode( tmpNode + 1, threads );

    numThreads = threads.size();
    for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
    {
      pair<PRV_UINT32, PRV_UINT32> blockPos;
      if ( traceIndex[ iThread ].findRecord( whichTime, blockPos ) )
      {
        block.push_back( blockPos.first );
        pos.push_back( blockPos.second );
      }
      else
      {
        block.push_back( 0 );
        pos.push_back( 0 );
      }
    }

    CPUIterator *tmpIt = new PlainTrace::CPUIterator( myBlocks, myTrace, block, pos, numThreads, threads, ii );

    while ( !tmpIt->isNull() && tmpIt->getTime() > whichTime )
      --( *tmpIt );
    if ( tmpIt->isNull() )
    {
      delete tmpIt;
      tmpIt = ( PlainTrace::CPUIterator * )CPUBegin( ii );
    }
    listIter[ ii ] = tmpIt;
  }
}

PlainTrace::iterator::iterator( PlainBlocks *whichBlocks, const Trace *whichTrace )
    :  MemoryTrace::iterator( whichTrace ), blocks( whichBlocks )
{
}

inline void PlainTrace::iterator::operator++()
{
}

inline void PlainTrace::iterator::operator--()
{
}

inline PlainTrace::iterator *PlainTrace::iterator::clone() const
{
  return new PlainTrace::iterator( *this );
}

inline TRecordType  PlainTrace::iterator::getType() const
{
  return ( ( TRecord * )record )->type;
}

inline TRecordTime  PlainTrace::iterator::getTime() const
{
  return ( ( TRecord * )record )->time;
}

inline TCPUOrder    PlainTrace::iterator::getCPU() const
{
  return ( ( TRecord * )record )->CPU;
}

inline TObjectOrder PlainTrace::iterator::getOrder() const
{
  return 0;
}

inline TObjectOrder PlainTrace::iterator::getThread() const
{
  return 0;
}

inline TEventType   PlainTrace::iterator::getEventType() const
{
  return ( ( TRecord * )record )->URecordInfo.eventRecord.type;
}

inline TSemanticValue PlainTrace::iterator::getEventValue() const
{
  TEventType myType = ( ( TRecord * )record )->URecordInfo.eventRecord.type;

  double tmpPrecision = myTrace->getEventTypePrecision( myType );
  if( tmpPrecision != 0.0 )
    return ( ( TRecord * )record )->URecordInfo.eventRecord.value * tmpPrecision;
  return ( ( TRecord * )record )->URecordInfo.eventRecord.value;
}

inline TEventValue    PlainTrace::iterator::getEventValueAsIs() const
{
  return ( ( TRecord * )record )->URecordInfo.eventRecord.value;
}

inline TState       PlainTrace::iterator::getState() const
{
  return ( ( TRecord * )record )->URecordInfo.stateRecord.state;
}

inline TRecordTime PlainTrace::iterator::getStateEndTime() const
{
  return ( ( TRecord * )record )->URecordInfo.stateRecord.endTime;
}

inline TCommID      PlainTrace::iterator::getCommIndex() const
{
  return ( ( TRecord * )record )->URecordInfo.commRecord.index;
}

inline void PlainTrace::iterator::setTime( const TRecordTime whichTime )
{
  ( ( TRecord * )record )->time = whichTime;
}

inline void PlainTrace::iterator::setType( const TRecordType whichType )
{
  ( ( TRecord * )record )->type = whichType;
}

inline void  PlainTrace::iterator::setStateEndTime( const TRecordTime whichEndTime )
{
  ( ( TRecord * )record )->URecordInfo.stateRecord.endTime = whichEndTime;
}


/**************************************************************************
 * MemoryTrace Inherited ThreadIterator.
 **************************************************************************/

PlainTrace::ThreadIterator::ThreadIterator( PlainBlocks *whichBlocks, const Trace *whichTrace, PRV_UINT32 whichBlock, PRV_UINT32 whichPos,
    TThreadOrder whichThread )
    : PlainTrace::iterator(  whichBlocks, whichTrace  ), thread( whichThread ), block( whichBlock ), pos( whichPos )
{
  record = &blocks->blocks[ thread ][ block ][ pos ];
  lastBlock = blocks->blocks[ thread ].size() - 1;
  lastPos = blocks->currentRecord[ thread ];
}

inline TThreadOrder PlainTrace::ThreadIterator::getThread() const
{
  return thread;
}

inline TObjectOrder PlainTrace::ThreadIterator::getOrder() const
{
  return thread;
}

inline void PlainTrace::ThreadIterator::operator++()
{
  if ( pos == lastPos && block == lastBlock )
  {
    record = nullptr;
    return;
  }

  if ( pos == PlainBlocks::blockSize - 1 )
  {
    ++block;
    pos = 0;
    record = &blocks->blocks[ thread ][ block ][ pos ];
    return;
  }

  ++pos;
  record += sizeof( TRecord );
}

inline void PlainTrace::ThreadIterator::operator--()
{
  if ( pos == 0 && block == 0  )
  {
    record = nullptr;
    return;
  }

  if ( pos == 0 )
  {
    pos = PlainBlocks::blockSize - 1;
    --block;
    record = &blocks->blocks[ thread ][ block ][ pos ];
    return;
  }

  --pos;
  record -= sizeof( TRecord );
}

MemoryTrace::iterator& PlainTrace::ThreadIterator::operator=( const MemoryTrace::iterator & copy )
{
  if ( this != &copy )
  {
    PlainTrace::ThreadIterator *tmpCopy = ( PlainTrace::ThreadIterator * ) & copy;
    thread = tmpCopy->thread;
    block = tmpCopy->block;
    pos = tmpCopy->pos;
    lastBlock = tmpCopy->lastBlock;
    lastPos = tmpCopy->lastPos;
    record = tmpCopy->getRecord();
  }
  return *this;
}

inline PlainTrace::ThreadIterator *PlainTrace::ThreadIterator::clone() const
{
  return new PlainTrace::ThreadIterator( *this );
}

/**************************************************************************
 * MemoryTrace Inherited CPUIterator.
 **************************************************************************/

PlainTrace::CPUIterator::CPUIterator( PlainBlocks *whichBlocks, const Trace *whichTrace, vector<PRV_UINT32>& whichBlock, vector<PRV_UINT32>& whichPos,
                                      TThreadOrder whichNumThreads, vector<TThreadOrder>& whichThreads, TCPUOrder whichCPU )
    : PlainTrace::iterator( whichBlocks, whichTrace ), cpu( whichCPU ), numThreads( whichNumThreads ),
    threads( whichThreads ), block( whichBlock ), pos( whichPos )
{
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    lastBlock.push_back( blocks->blocks[ threads[ iThread ] ].size() - 1 );
    lastPos.push_back( blocks->currentRecord[ threads[ iThread ] ] + 1 );
  }
  lastThread = minThread();
  record = &blocks->blocks[ threads[ lastThread ] ][ block[ lastThread ] ][ pos[ lastThread ] ];
}

inline TThreadOrder PlainTrace::CPUIterator::getThread() const
{
  return lastThread;
}

inline TObjectOrder PlainTrace::CPUIterator::getOrder() const
{
  return cpu;
}

inline void PlainTrace::CPUIterator::operator++()
{
  TThreadOrder inLastPos = 0;
  ++pos[ lastThread ];
  if ( pos[ lastThread ] >= PlainBlocks::blockSize )
  {
    if ( block[ lastThread ] < lastBlock[ lastThread ] )
    {
      ++block[ lastThread ];
      pos[ lastThread ] = 0;
    }
  }
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    if ( pos[ iThread ] == lastPos[ iThread ] && block[ iThread ] == lastBlock[ iThread ] )
    {
      ++inLastPos;
    }
  }
  if ( inLastPos == numThreads )
  {
    record = nullptr;
    return;
  }

  lastThread = minThread();
  record = &blocks->blocks[ threads[ lastThread ] ][ block[ lastThread ] ][ pos[ lastThread ] ];
}

inline void PlainTrace::CPUIterator::operator--()
{
  TThreadOrder inFirstPos = 0;
  if ( pos[ lastThread ] == 0 )
  {
    if ( block[ lastThread ] > 0 )
    {
      pos[ lastThread ] = PlainBlocks::blockSize - 1;
      --block[ lastThread ];
    }
  }
  else
    --pos[ lastThread ];
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    if ( pos[ iThread ] == 0 && block[ iThread ] == 0 )
      ++inFirstPos;
  }
  if ( inFirstPos == numThreads )
  {
    record = nullptr;
    return;
  }

  lastThread = maxThread();
  record = &blocks->blocks[ threads[ lastThread ] ][ block[ lastThread ] ][ pos[ lastThread ] ];
}

MemoryTrace::iterator& PlainTrace::CPUIterator::operator=( const MemoryTrace::iterator & copy )
{
  if ( this != &copy )
  {
    PlainTrace::CPUIterator *tmpCopy = ( PlainTrace::CPUIterator * ) & copy;
    cpu = tmpCopy->cpu;
    numThreads = tmpCopy->numThreads;
    threads = tmpCopy->threads;
    block = tmpCopy->block;
    pos = tmpCopy->pos;
    lastBlock = tmpCopy->lastBlock;
    lastPos = tmpCopy->lastPos;
    lastThread = tmpCopy->lastThread;
    record = tmpCopy->getRecord();
  }
  return *this;
}

inline PlainTrace::CPUIterator *PlainTrace::CPUIterator::clone() const
{
  return new PlainTrace::CPUIterator( *this );
}

inline TThreadOrder PlainTrace::CPUIterator::minThread()
{
  map<TRecord *, TThreadOrder, Plain::ltrecord> sortedRecords;

  setToMyCPUForward();
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    if ( !( pos[ iThread ] == lastPos[ iThread ] && block[ iThread ] == lastBlock[ iThread ] ) )
      sortedRecords[ &blocks->blocks[ threads[ iThread ] ][ block[ iThread ] ][ pos[ iThread ] ] ] = iThread;
  }

  if ( sortedRecords.begin() == sortedRecords.end() )
    return 0;

  return sortedRecords.begin()->second;
}

inline TThreadOrder PlainTrace::CPUIterator::maxThread()
{
  map<TRecord *, TThreadOrder, Plain::ltrecord> sortedRecords;

  setToMyCPUBackward();
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    if ( !( pos[ iThread ] == 0 && block[ iThread ] == 0 ) )
      sortedRecords[ &blocks->blocks[ threads[ iThread ] ][ block[ iThread ] ][ pos[ iThread ] ] ] = iThread;
  }

  if ( sortedRecords.begin() == sortedRecords.end() )
    return 0;

  map<TRecord *, TThreadOrder, ltrecord>::iterator it = sortedRecords.end();
  --it;
  return it->second;
}

inline void PlainTrace::CPUIterator::setToMyCPUForward()
{
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    if ( !( pos[ iThread ] == lastPos[ iThread ] && block[ iThread ] == lastBlock[ iThread ] ) )
    {
      TRecord *tmpRec = &blocks->blocks[ threads[ iThread ] ][ block[ iThread ] ][ pos[ iThread ] ];
      if( tmpRec->type == EMPTYREC )
        return;
      while ( !( pos[ iThread ] >= lastPos[ iThread ] && block[ iThread ] >= lastBlock[ iThread ] )
              && tmpRec->CPU != cpu )
      {
        ++pos[ iThread ];
        if ( pos[ iThread ] >= PlainBlocks::blockSize )
        {
          if ( block[ iThread ] < lastBlock[ iThread ] )
          {
            ++block[ iThread ];
            pos[ iThread ] = 0;
          }
        }
        tmpRec = &blocks->blocks[ threads[ iThread ] ][ block[ iThread ] ][ pos[ iThread ] ];
      }
    }
  }
}

inline void PlainTrace::CPUIterator::setToMyCPUBackward()
{
  for ( TThreadOrder iThread = 0; iThread < numThreads; ++iThread )
  {
    if ( !( pos[ iThread ] == 0 && block[ iThread ] == 0 ) )
    {
      TRecord *tmpRec = &blocks->blocks[ threads[ iThread ] ][ block[ iThread ] ][ pos[ iThread ] ];
      if( tmpRec->type == EMPTYREC )
        return;
      while ( !( pos[ iThread ] == 0 && block[ iThread ] == 0 ) && tmpRec->CPU != cpu )
      {
        if ( pos[ iThread ] == 0 )
        {
          if( block[ iThread ] > 0 )
          {
            --block[ iThread ];
            pos[ iThread ] = PlainBlocks::blockSize - 1;
          }
        }
        else
          --pos[ iThread ];
        tmpRec = &blocks->blocks[ threads[ iThread ] ][ block[ iThread ] ][ pos[ iThread ] ];
      }
    }
  }
}
