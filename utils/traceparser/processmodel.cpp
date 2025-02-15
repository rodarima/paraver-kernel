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


#include <sstream>
#include <iostream>
#include <fstream>
#include <limits>
#include <numeric>

#include "processmodel.h"
#ifdef USE_PARAVER_EXCEPTIONS
#include "traceheaderexception.h"
#endif

using namespace std;

#ifdef _WIN32
#undef max
#undef min
#endif

// subset
template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
bool ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::operator<( const ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >& other ) const
{
  bool isSubset = true;

  ApplOrderT iAppl;
  TaskOrderT iTask;
  ThreadOrderT iThread;
  for ( ThreadOrderT iGlobalThread = 0; iGlobalThread < totalThreads(); ++iGlobalThread )
  {
    getThreadLocation( iGlobalThread, iAppl, iTask, iThread );
    if ( !other.isValidThread( iAppl, iTask, iThread ) )
    {
      isSubset = false;
      break;
    }
  }

  return isSubset;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
bool ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::operator==( const ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >& other ) const
{
  // ORIGINAL: QUICK BAD
/*  return applications == other.applications &&
         tasks        == other.tasks        &&
         threads      == other.threads;*/


  // ELEGANT SLOW
  // return ( ( *this < other ) && ( other < *this ) );

  return ( ( totalThreads() == other.totalThreads() ) && ( *this < other ) );
}



template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ApplOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::totalApplications() const
{
  return applications.size();
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
TaskOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::totalTasks() const
{
  return tasks.size();
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
void ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getTaskLocation( TaskOrderT globalTask,
                                    ApplOrderT& inAppl,
                                    TaskOrderT& inTask ) const
{
  inAppl = tasks[ globalTask ].appl;
  inTask = tasks[ globalTask ].task;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
TaskOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getGlobalTask( const ApplOrderT& inAppl,
                                        const TaskOrderT& inTask ) const
{
  return applications[ inAppl ].tasks[ inTask ].traceGlobalOrder;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ThreadOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::totalThreads() const
{
  return threads.size();
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ThreadOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::totalThreads( ApplOrderT whichAppl ) const
{
  return std::accumulate( applications[ whichAppl ].tasks.begin(),
                          applications[ whichAppl ].tasks.end(),
                          0,
                          []( int a, const ProcessModelTask< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT > &b ){ return a + b.threads.size(); } );
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ThreadOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getGlobalThread( const ApplOrderT& inAppl,
    const TaskOrderT& inTask,
    const ThreadOrderT& inThread ) const
{
  return applications[ inAppl ].tasks[ inTask ].threads[ inThread ].traceGlobalOrder;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
void ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getThreadLocation( ThreadOrderT globalThread,
                                      ApplOrderT& inAppl,
                                      TaskOrderT& inTask,
                                      ThreadOrderT& inThread ) const
{
  //////////////////////////////////////
  inAppl = threads[ globalThread ].appl;
  inTask = threads[ globalThread ].task;
  inThread = threads[ globalThread ].thread;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::ProcessModel( istringstream& headerInfo, bool existResourceInfo )
{
  ApplOrderT numberApplications;
  TaskOrderT globalTasks = 0;
  ThreadOrderT globalThreads = 0;
  ready = false;

  string stringNumberApplications;
  std::getline( (istream&)headerInfo, stringNumberApplications, ':' );
  istringstream sstreamNumberAppl( stringNumberApplications );

  if ( !( sstreamNumberAppl >> numberApplications ) ||
       numberApplications == 0 )
  {
#ifdef USE_PARAVER_EXCEPTIONS
    throw TraceHeaderException( TTraceHeaderErrorCode::invalidApplNumber,
                                stringNumberApplications.c_str() );
#else
    std::cerr << "[Error] Invalid applications number: " << stringNumberApplications << std::endl;
    exit( 1 );
#endif
  }

  // Insert applications
  for ( ApplOrderT countAppl = 0; countAppl < numberApplications; ++countAppl )
  {
    TaskOrderT numberTasks;

    applications.push_back( ProcessModelAppl< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >( countAppl ) );

    string stringNumberTasks;
    std::getline( headerInfo, stringNumberTasks, '(' );
    istringstream sstreamNumberTasks( stringNumberTasks );

    if ( !( sstreamNumberTasks >> numberTasks ) ||
         numberTasks == 0 )
    {
#ifdef USE_PARAVER_EXCEPTIONS
      throw TraceHeaderException( TTraceHeaderErrorCode::invalidTaskNumber,
                                  stringNumberTasks.c_str() );
#else
      std::cerr << "[Error] Invalid task number: " << stringNumberTasks << std::endl;
      exit( 1 );
#endif
    }

    // Insert tasks
    for ( TaskOrderT countTask = 0; countTask < numberTasks; ++countTask )
    {
      ThreadOrderT numberThreads;
      NodeOrderT numberNode;

      if( globalTasks < std::numeric_limits<TaskOrderT>::max() )
      {
        applications[ countAppl ].tasks.push_back( ProcessModelTask< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >( globalTasks ) );
        tasks.push_back( TaskLocation() );
        tasks[ globalTasks ].appl = countAppl;
        tasks[ globalTasks ].task = countTask;
        ++globalTasks;
      }
      string stringNumberThreads;
      std::getline( headerInfo, stringNumberThreads, ':' );
      istringstream sstreamNumberThreads( stringNumberThreads );

      if ( !( sstreamNumberThreads >> numberThreads ) ||
           numberThreads == 0 )
      {
#ifdef USE_PARAVER_EXCEPTIONS
        throw TraceHeaderException( TTraceHeaderErrorCode::invalidThreadNumber,
                                    stringNumberThreads.c_str() );
#else
      std::cerr << "[Error] Invalid thread number: " << stringNumberThreads << std::endl;
      exit( 1 );
#endif
      }

      string stringNumberNode;

      if ( countTask < numberTasks - 1 )
        std::getline( headerInfo, stringNumberNode, ',' );
      else
        std::getline( headerInfo, stringNumberNode, ')' );

      istringstream sstreamNumberNode( stringNumberNode );

      if ( !( sstreamNumberNode >> numberNode ) ||
           ( numberNode == 0 && existResourceInfo ) )
      {
#ifdef USE_PARAVER_EXCEPTIONS
        throw TraceHeaderException( TTraceHeaderErrorCode::invalidNodeNumber,
                                    stringNumberNode.c_str() );
#else
        std::cerr << "[Error] Invalid node number: " << stringNumberNode << std::endl;
        exit( 1 );
#endif
      }

      // Insert threads
      for ( ThreadOrderT countThread = 0; countThread < numberThreads; ++countThread )
      {
        if( globalThreads < std::numeric_limits<ThreadOrderT>::max() )
        {
          applications[ countAppl ].tasks[ countTask ].threads.push_back(
              ProcessModelThread< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >( globalThreads, numberNode - 1 ) );
          threads.push_back( ThreadLocation() );
          threads[ globalThreads ].appl = countAppl;
          threads[ globalThreads ].task = countTask;
          threads[ globalThreads ].thread = countThread;

          typename map< NodeOrderT, vector< ThreadOrderT > >::iterator nodeIt = threadsPerNode.find( numberNode - 1 );
          if( nodeIt == threadsPerNode.end() )
            threadsPerNode[ numberNode - 1 ] = vector<ThreadOrderT>();
          threadsPerNode[ numberNode - 1 ].push_back( globalThreads );

          ++globalThreads;
        }
      }
      // End inserting threads
    }
    // End inserting tasks

    // Gets a useless character: ':' or ','
    headerInfo.get();

  }
  // End inserting applications

  ready = true;
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
void ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::dumpToFile( fstream& file, bool existResourceInfo ) const
{
  ostringstream ostr;
  ostr << fixed;
  ostr << dec;
  ostr.precision( 0 );

  ostr << applications.size() << ':';
  for ( ApplOrderT iAppl = 0; iAppl < applications.size(); ++iAppl )
  {
    ostr << applications[ iAppl ].tasks.size() << '(';
    for ( TaskOrderT iTask = 0; iTask < applications[ iAppl ].tasks.size(); ++iTask )
    {
      ostr << applications[ iAppl ].tasks[ iTask ].threads.size() << ':';
      if( existResourceInfo )
        ostr << applications[ iAppl ].tasks[ iTask ].threads[ 0 ].nodeExecution + 1;
      else
        ostr << "0";

      if ( iTask < applications[ iAppl ].tasks.size() - 1 )
        ostr << ',';
    }
    ostr << ')';

    if ( iAppl < applications.size() - 1 )
      ostr << ':';
  }
  file << ostr.str();
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
TaskOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getFirstTask( ApplOrderT inAppl ) const
{
  return applications[ inAppl ].tasks[ 0 ].traceGlobalOrder;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
TaskOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getLastTask( ApplOrderT inAppl ) const
{
  return applications[ inAppl ].tasks[
           applications[ inAppl ].tasks.size() - 1 ].traceGlobalOrder;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ThreadOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getFirstThread( ApplOrderT inAppl, TaskOrderT inTask ) const
{
  return applications[ inAppl ].tasks[ inTask ].threads[ 0 ].traceGlobalOrder;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ThreadOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getLastThread( ApplOrderT inAppl, TaskOrderT inTask )const
{
  return applications[ inAppl ].tasks[ inTask ].threads[
           applications[ inAppl ].tasks[ inTask ].threads.size() - 1 ].traceGlobalOrder;
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
void ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::getThreadsPerNode( NodeOrderT inNode, vector<ThreadOrderT>& onVector ) const
{
  onVector.clear();

  if( inNode == 0 ) return;

  typename map< NodeOrderT, vector< ThreadOrderT > >::const_iterator nodeIt = threadsPerNode.find( inNode - 1 );
  if( nodeIt != threadsPerNode.end() )
    onVector = nodeIt->second;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
bool ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::isValidThread( ThreadOrderT whichThread ) const
{
  return whichThread < threads.size();
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
bool ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::isValidThread( ApplOrderT whichAppl,
                                  TaskOrderT whichTask,
                                  ThreadOrderT whichThread ) const
{
  if( !isValidAppl( whichAppl ) )
    return false;
  if( whichTask >= applications[ whichAppl ].tasks.size() )
    return false;

  return whichThread < applications[ whichAppl ].tasks[ whichTask ].threads.size();
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
bool ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::isValidTask( TaskOrderT whichTask ) const
{
  return whichTask < tasks.size();
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
bool ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::isValidTask( ApplOrderT whichAppl,
                                TaskOrderT whichTask ) const
{
  if( !isValidAppl( whichAppl ) )
    return false;

  return whichTask < applications[ whichAppl ].tasks.size();
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
bool ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::isValidAppl( ApplOrderT whichAppl ) const
{
  return whichAppl < applications.size();
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ApplOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::addApplication(  )
{
  applications.emplace_back( applications.size() );
  return applications.size() - 1;
}

template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
TaskOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::addTask( ApplOrderT whichAppl )
{
  if( whichAppl > applications.size() )
  {
    stringstream tmpstr;
    tmpstr << whichAppl;
#ifdef USE_PARAVER_EXCEPTIONS
    throw TraceHeaderException( TTraceHeaderErrorCode::invalidApplNumber,
                                tmpstr.str().c_str() );
#else
    std::cerr << "[Error] Invalid header time: " << tmpstr.str() << std::endl;
    exit( 1 );
#endif
  }

  tasks.push_back( TaskLocation() );
  tasks.back().appl = whichAppl;
  tasks.back().task = applications[ whichAppl ].tasks.size();
  applications[ whichAppl ].tasks.emplace_back( tasks.size() - 1 );

  return tasks.back().task;
}


template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ThreadOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::addThread( ApplOrderT whichAppl, TaskOrderT whichTask,
                              NodeOrderT execNode )
{
  if( whichAppl > applications.size() )
  {
    stringstream tmpstr;
    tmpstr << whichAppl;
#ifdef USE_PARAVER_EXCEPTIONS
    throw TraceHeaderException( TTraceHeaderErrorCode::invalidApplNumber,
                                tmpstr.str().c_str() );
#else
    std::cerr << "[Error] Invalid application number: " << tmpstr.str() << std::endl;
    exit( 1 );
#endif
  }
  else if( whichTask > applications[ whichAppl ].tasks.size() )
  {
    stringstream tmpstr;
    tmpstr << whichAppl;
#ifdef USE_PARAVER_EXCEPTIONS
    throw TraceHeaderException( TTraceHeaderErrorCode::invalidTaskNumber,
                                tmpstr.str().c_str() );
#else
    std::cerr << "[Error] Invalid task number: " << tmpstr.str() << std::endl;
    exit( 1 );
#endif
  }

  threads.push_back( ThreadLocation() );
  threads.back().appl = whichAppl;
  threads.back().task = whichTask;
  threads.back().thread = applications[ whichAppl ].tasks[ whichTask ].threads.size();
  applications[ whichAppl ].tasks[ whichTask ].threads.emplace_back( threads.size() - 1, execNode );

  return threads.back().thread;
}



template< typename ApplOrderT, 
          typename TaskOrderT, 
          typename ThreadOrderT, 
          typename NodeOrderT >
ThreadOrderT ProcessModel< ApplOrderT, TaskOrderT, ThreadOrderT, NodeOrderT >::addApplTaskThread( const ThreadLocation& whichLocation,
                                      NodeOrderT execNode )
{
  if( whichLocation.appl >= applications.size() )
    addApplication();

  if( whichLocation.task >= applications[ whichLocation.appl ].tasks.size() )
    addTask( whichLocation.appl );

  threads.push_back( whichLocation );
  applications[ whichLocation.appl ].tasks[ whichLocation.task ].threads.emplace_back( threads.size() - 1, execNode );

  return threads.size() - 1;
}
