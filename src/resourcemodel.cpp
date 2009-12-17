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

#include <sstream>
#include <iostream>
#include <fstream>
#include "resourcemodel.h"
#include "traceheaderexception.h"


TNodeOrder ResourceModel::totalNodes() const
{
  return nodes.size();
}


TCPUOrder ResourceModel::totalCPUs() const
{
  return CPUs.size();
}


TCPUOrder ResourceModel::getGlobalCPU( const TNodeOrder& inNode,
                                       const TCPUOrder& inCPU ) const
{
  return nodes[ inNode ].CPUs[ inCPU ].traceGlobalOrder;
}


void ResourceModel::getCPULocation( TCPUOrder globalCPU,
                                    TNodeOrder& inNode,
                                    TCPUOrder& inCPU ) const
{
  inNode = CPUs[ globalCPU ].node;
  inCPU = CPUs[ globalCPU ].CPU;
}


TCPUOrder ResourceModel::getFirstCPU( TNodeOrder inNode ) const
{
  return nodes[ inNode ].CPUs[ 0 ].traceGlobalOrder;
}


TCPUOrder ResourceModel::getLastCPU( TNodeOrder inNode ) const
{
  return nodes[ inNode ].CPUs[
           nodes[ inNode ].CPUs.size() - 1 ].traceGlobalOrder;
}


ResourceModel::ResourceModel( istringstream& headerInfo )
{
  string stringNumberNodes;
  TNodeOrder numberNodes;
  TCPUOrder globalCPUs = 0;
  bool readCPUs;
  ready = false;

  // Number of nodes is 0 -> No definition of nodes nor cpus
  if ( headerInfo.peek() == '0' )
  {
    std::getline( headerInfo, stringNumberNodes, ':' );
    return;
  }

  if ( headerInfo.str().find_first_of( '(', headerInfo.tellg() ) <
       headerInfo.str().find_first_of( ':', headerInfo.tellg() ) )
  {
    std::getline( headerInfo, stringNumberNodes, '(' );
    readCPUs = true;
  }
  else
  {
    std::getline( headerInfo, stringNumberNodes, ':' );
    readCPUs = false;
  }
  istringstream sstreamNumberNodes( stringNumberNodes );

  if ( !( sstreamNumberNodes >> numberNodes ) )
  {
    throw TraceHeaderException( TraceHeaderException::invalidNodeNumber,
                                stringNumberNodes.c_str() );
  }

  // Insert nodes
  for ( TNodeOrder countNode = 0; countNode < numberNodes; countNode++ )
  {
    TCPUOrder numberCPUs;

    nodes.push_back( ResourceModelNode( countNode ) );

    if ( readCPUs )
    {
      string stringNumberCPUs;
      if ( countNode < numberNodes - 1 )
        std::getline( headerInfo, stringNumberCPUs, ',' );
      else
        std::getline( headerInfo, stringNumberCPUs, ')' );

      istringstream sstreamNumberCPUs( stringNumberCPUs );

      if ( !( sstreamNumberCPUs >> numberCPUs ) )
      {
        throw TraceHeaderException( TraceHeaderException::invalidCPUNumber,
                                    stringNumberCPUs.c_str() );
      }
    }
    else
      numberCPUs = 1;

    // Insert CPUs
    for ( TCPUOrder countCPU = 0; countCPU < numberCPUs; countCPU++ )
    {
      nodes[ countNode ].CPUs.push_back( ResourceModelCPU( globalCPUs ) );
      CPUs.push_back( CPULocation() );
      CPUs[ globalCPUs ].node = countNode;
      CPUs[ globalCPUs ].CPU = countCPU;
      globalCPUs++;
    }
    // End inserting CPUs

  }
  // End inserting nodes

  // Gets a useless character: ':'
  if ( readCPUs )
    headerInfo.get();

  ready = true;
}

void ResourceModel::dumpToFile( fstream& file ) const
{
  ostringstream ostr;
  ostr << fixed;
  ostr << dec;
  ostr.precision( 0 );

  if ( !ready )
  {
    ostr << '0';
  }
  else
  {
    ostr << nodes.size() << '(';
    for ( TNodeOrder iNode = 0; iNode < nodes.size(); ++iNode )
    {
      ostr << nodes[ iNode ].CPUs.size();
      if ( iNode < nodes.size() - 1 )
        ostr << ',';
    }
    ostr << ')';
  }
  file << ostr.str();
}
