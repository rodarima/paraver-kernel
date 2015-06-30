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

#include "workspacemanager.h"

using std::string;
using std::vector;

WorkspaceManager *WorkspaceManager::instance = NULL;

WorkspaceManager *WorkspaceManager::getInstance()
{
  if ( WorkspaceManager::instance == NULL )
    WorkspaceManager::instance = new WorkspaceManager();
  return WorkspaceManager::instance;
}

WorkspaceManager::WorkspaceManager()
{
  // delete me
  Workspace tmp;

  std::pair< string, string > tmpwkr(  "/home/pgonzalez/install/wxparaver/latest/cfgs/mpi/views/MPI_call.cfg", "mPI CALLS!!" );
  string tmpStr( "MPI" );
  tmp.setName( tmpStr );
  tmp.addHintCFG( tmpwkr );
  addWorkspace( tmp );

  tmpwkr = std::pair< string, string >( "/home/pgonzalez/install/wxparaver/latest/cfgs/counters_PAPI/performance/IPC.cfg", "IPCIPC!!" );
  tmpStr = string( "PAPY" );
  tmp.clearHintCFGs();
  tmp.setName( tmpStr );
  tmp.addHintCFG( tmpwkr );
  addWorkspace( tmp );
}

WorkspaceManager::~WorkspaceManager()
{
}

void WorkspaceManager::clear()
{
  workspaces.clear();
}

vector<string> WorkspaceManager::getWorkspaces() const
{
  return workspacesOrder;
}

Workspace& WorkspaceManager::getWorkspace( std::string name )
{
  return workspaces[ name ];
}

void WorkspaceManager::addWorkspace( std::string whichName )
{
  workspaces[ whichName ] = Workspace();
}

void WorkspaceManager::addWorkspace( Workspace& whichWorkspace )
{
  workspaces[ whichWorkspace.getName() ] = whichWorkspace;
}
