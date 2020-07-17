#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <mpd/client.h>
#include <mpd/connection.h>
#include "mpdobserver.h"
#include "utils.h"

#define MPD_WAIT_SLEEP_TIME	10 //seconds
#define MPD_CONNECTION_TIMEOUT (10*60*1000) //milliseconds

typedef struct mpd_connection mpd_Connection;
static mpd_Connection	*mpdServerConnection = NULL;
static pthread_t		mpdStatusThread;

enum mpd_idle idleMask = MPD_IDLE_MIXER | MPD_IDLE_PLAYER | MPD_IDLE_STICKER | MPD_IDLE_QUEUE | MPD_IDLE_STORED_PLAYLIST;

typedef struct _mpd_server {
	char serverName[ 100 ];
	uint16_t serverPort;
} mpd_Server;

static mpd_Server mpdServer;
static int radioServerQuit = 0;

void *mpdStatusThreadProc( void *arg )
{
	enum mpd_idle changeType;
	int change;
	int pipeFD = (intptr_t)arg;
	int writeRes;
	int secCounter = 0;

//	mpdServerConnection = mpd_connection_new( mpdServer.serverName, mpdServer.serverPort, MPD_CONNECTION_TIMEOUT );
	mpdServerConnection = NULL;

	while( !radioServerQuit )
	{
		//at the begining the connection is null
		//if we lose connection to the server then changeType, returned by mpd_run_idle would be 0
		//and finally we might get some error from the server
		if(		!mpdServerConnection || 
				mpd_connection_get_error( mpdServerConnection ) == MPD_ERROR_CLOSED || 
				mpd_connection_get_error( mpdServerConnection ) == MPD_ERROR_SERVER || 
				mpd_connection_get_error( mpdServerConnection ) == MPD_ERROR_TIMEOUT 
		)
		{
			if( mpdServerConnection )
			{
				printf( "MPD IDLE supervisor problem: %s\n", mpd_connection_get_error_message( mpdServerConnection ) );
				mpd_connection_free( mpdServerConnection );
				mpdServerConnection = NULL;
				change = MPD_CLIENT_DISCONNECT;
				writeRes = write( pipeFD, &change, sizeof( change ) );
			}

			printf( "Connection to MPD server for IDLE supervising...\n" );

			do
			{
				if( radioServerQuit )
				{
					break;
				}
				if( secCounter )
				{
					printf( "." );
					sleep( 1 );
					secCounter--;
				}
				else
				{
					mpdServerConnection = mpd_connection_new( mpdServer.serverName, mpdServer.serverPort, MPD_CONNECTION_TIMEOUT );
					if( !mpdServerConnection || mpd_connection_get_error( mpdServerConnection ) != MPD_ERROR_SUCCESS )
					{
						printf( "MPD IDLE supervisor problem: %s\n", mpd_connection_get_error_message( mpdServerConnection ) );
						mpd_connection_free( mpdServerConnection );
						mpdServerConnection = NULL;
					}
					//available only in 2.11
					//mpd_connection_set_keepalive( mpdServerConnection, true );
					secCounter = MPD_WAIT_SLEEP_TIME;
				}
			} while( !mpdServerConnection );

			change = MPD_CLIENT_CONNECT;
			writeRes = write( pipeFD, &change, sizeof( change ) );
		}

		changeType = 0;
		if( mpd_send_idle_mask( mpdServerConnection, idleMask ) )
		{
			changeType = mpd_recv_idle( mpdServerConnection, false );
		}

		if( changeType )
		{
			change = (int)changeType;
			writeRes = write( pipeFD, &change, sizeof( change ) );
		}
	}
	if( mpdServerConnection )
	{
		mpd_connection_free( mpdServerConnection );
	}
	pthread_detach( mpdStatusThread );
	return NULL;
}

void radio_mpd_connect( const char *server, const char *port, int pipeFD )
{
	strncpy( mpdServer.serverName, server, sizeof( mpdServer.serverName ) );
	mpdServer.serverPort = atoi( port );

	//radio_mpd_callback = statusChanged;
	pthread_create( &mpdStatusThread, NULL, &mpdStatusThreadProc, (void *)(intptr_t)pipeFD );
}

void radio_mpd_close( void )
{
	radioServerQuit = 1;
	radio_force_refresh();
	pthread_join( mpdStatusThread, NULL );
}

//will force the blocked thread waiting for an event to recheck
void radio_force_refresh( void )
{
	if( mpdServerConnection )
	{
		mpd_send_noidle( mpdServerConnection );
	}
}
