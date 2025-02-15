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


#include "tracestream.h"
#include "paraverkernelexception.h"
#include <iostream>
#ifndef _WIN32
#include <stdlib.h>
#endif

using namespace std;

const double TraceStream::GZIP_COMPRESSION_RATIO = 6.3;

TraceStream *TraceStream::openFile( const string& filename )
{
  string strExt = filename.substr( filename.length() - 3 );

  if ( strExt.compare( ".gz" ) == 0 )
    return new Compressed( filename );
  else
    return new NotCompressed( filename );
}

TTraceSize TraceStream::getTraceFileSize( const string& filename )
{
  string strExt = filename.substr( filename.length() - 3 );

  if ( strExt.compare( ".gz" ) == 0 )
    return NotCompressed::getTraceFileSize( filename ) * GZIP_COMPRESSION_RATIO;
  else
    return NotCompressed::getTraceFileSize( filename );
}

std::string TraceStream::getFilename() const
{
  return filename;
}


void TraceStream::setFilename( const std::string &newFile )
{
  filename = newFile;
}




NotCompressed::NotCompressed( const string& filename )
{
  setFilename( filename );
  file.open( filename.c_str() );
}

void NotCompressed::open( const string& filename )
{
  file.open( filename.c_str() );
}

void NotCompressed::close()
{
  file.close();
}

void NotCompressed::getline( string& strLine )
{
  std::getline( file, strLine );
}

bool NotCompressed::eof()
{
  return file.eof();
}

void NotCompressed::seekbegin()
{
  file.seekg( 0, ios_base::beg );
}

void NotCompressed::seekend()
{
  file.seekg( 0, ios_base::end );
}

void NotCompressed::seekg( streampos pos )
{
  file.seekg( pos );
}

streampos NotCompressed::tellg()
{
  return file.tellg();
}

bool NotCompressed::canseekend()
{
  return true;
}

bool NotCompressed::good() const
{
  return file.good();
}

void NotCompressed::clear()
{
  file.clear();
}

int NotCompressed::peek()
{
  return file.peek();
}

TTraceSize NotCompressed::getTraceFileSize( const string& filename )
{
  FILE *traceFile;
  TTraceSize tmpSize;

#if defined (__FreeBSD__) || defined(__APPLE__)
	if ( (traceFile = fopen (filename.c_str(), "r" ) ) == nullptr )
	{
		printf ( "Error Opening File %s\n", filename.c_str() );
		return 0;
	}

#elif defined(_WIN32)
  if ( fopen_s( &traceFile, filename.c_str(), "r" ) != 0 )
  {
    printf( "Error Opening File %s\n", filename.c_str() );
    return 0;
  }
#else
  if ( ( traceFile = fopen64( filename.c_str(), "r" ) ) == nullptr )
  {
    printf( "Error Opening File %s\n", filename.c_str() );
    return 0;
  }
#endif

#ifdef _WIN32
    _fseeki64( traceFile, 0, SEEK_END );
#else
    fseek( traceFile, 0, SEEK_END );
#endif

#ifdef _WIN32
    tmpSize = _ftelli64( traceFile );
#else
    tmpSize = ftell( traceFile );
#endif

  fclose( traceFile );
  return tmpSize;
}




Compressed::Compressed( const string& filename )
{
  setFilename( filename );
  file = gzopen( filename.c_str(), "r" );
}

void Compressed::open( const string& filename )
{
  file = gzopen( filename.c_str(), "r" );
}

void Compressed::close()
{
  gzclose( file );
}

void Compressed::getline( string& strLine )
{
#ifndef _MSC_VER
  gzgets( file, tmpLine, LINESIZE );
#else
  throw ParaverKernelException( ParaverKernelException::gzipNotSupported );
  int c;
  c = gzgetc( file );
  PRV_UINT32 i = 0;
  while( c != '\n' )
  {
    if( c == '\r' )
      continue;
    if( c == -1 )
      break;
    tmpLine[ i ] = c;
    c = gzgetc( file );
    ++i;
  }
  tmpLine[ i ] = '\0';
#endif
  strLine = tmpLine;
  if ( strLine.back() == '\n' )
    strLine.pop_back();
}

bool Compressed::eof()
{
  return gzeof( file );
}

void Compressed::seekbegin()
{
  gzseek( file, 0, SEEK_SET );
}

void Compressed::seekend()
{
  gzseek( file, 0, SEEK_END );
}

void Compressed::seekg( streampos pos )
{
  gzseek( file, pos, SEEK_SET );
}

streampos Compressed::tellg()
{
  return gztell( file );
}

bool Compressed::canseekend()
{
  return false;
}

bool Compressed::good() const
{
  return file != nullptr;
}

void Compressed::clear()
{}

int Compressed::peek()
{
  return gzgetc( file );
}

TTraceSize Compressed::getTraceFileSize( const string& filename )
{
  TTraceSize tmpSize;

  Compressed tmpComp( filename );
  if( !tmpComp.good() )
  {
    tmpComp.close();
    return 0;
  }
  tmpComp.seekend();
  tmpSize = tmpComp.tellg();
  tmpComp.close();

  return tmpSize;
}
