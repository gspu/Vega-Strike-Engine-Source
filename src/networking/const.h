/* 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
  Constants - Network Interface Constants - written by Stephane Vaxelaire <svax@free.fr>
*/

#ifndef __CONST_H
#define __CONST_H

#include "config.h"
#include "endianness.h"
#include <assert.h>
#include <stdio.h>

#define GAMESERVER_VERSION 0.2
#define ACCOUNTSERVER_VERSION 0.2
#define NETCLIENT_VERSION 0.2

#define MAXCLIENTS	512
#define MAXOBJECTS	1024 // Maximum number of objects in a zone - HAVE TO FIND A WAY NOT TO LIMIT THAT
#define SERVER_PORT 6777
#define CLIENT_PORT	6778
#define ACCT_PORT	6779
#define NAMELEN		32
#define MAXBUFFER	16384

// Communication freq range
#define MIN_COMMFREQ	23.0
#define MAX_COMMFREQ	42.0

#define MAXSERIAL 0xFFFF
#define OBJSERIAL_TONET VSSwapHostShortToLittle
#define INSTSERIAL_TONET VSSwapHostIntToLittle
typedef unsigned short ObjSerial;
typedef unsigned int InstSerial;

extern double NETWORK_ATOM;
extern double DAMAGE_ATOM;

#if defined(_WIN32) && !defined(__CYGWIN__) || defined(__APPLE__)
	//#warning "Win32 platform"
	#define in_addr_t unsigned long
	#define socklen_t int
#else
	//#warning "GCC platform"
  #ifndef SOCKET_ERROR
  #define SOCKET_ERROR -1
  #endif
#endif

#if !defined( _WIN32) || defined( __CYGWIN__)
  #define LOCALCONST_DECL(Type,Name,Value) static const Type Name = Value;
  #define LOCALCONST_DEF(Class,Type,Name,Value)
#else
  #define LOCALCONST_DECL(Type,Name,Value) static Type Name;
  #define LOCALCONST_DEF(Class,Type,Name,Value) Type Class::Name = Value;
#endif

#endif /* __CONST_H */

