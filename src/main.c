/*
 * References:
 * MusicTracker : https://github.com/petervizi/musictracker/blob/master/src/mpd.c
 * MPDCron : https://github.com/alip/mpdcron
 * Jansson memory leak : https://groups.google.com/forum/#!topic/jansson-users/xD8QLQF3ex8
 *
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <fcntl.h>
#include <mpd/client.h>
#include <jansson.h>
#include "mongoose.h"
#include "mpdobserver.h"
#include "utils.h"
#include "config.h"

#define NAME "RADIOSERVER"
#define CFG_NAME "mpdweb"
#define WS_CONNECT_MESSAGE "listenformpd"
#define STATIC_DIR "./static"

#define REST_NUMBER_SIZE 20

#define HANDLE( cmpfn, restURI, method, handler ) \
else \
if( cmpfn( conn->uri, restURI ) && !strcmp( conn->request_method, method ) ) \
{ \
	handler( conn ); \
	return MG_TRUE; \
}

#define HANDLE_START( restURI, method, handler )	HANDLE( strstr, restURI, method, handler )
#define HANDLE_EXACT( restURI, method, handler )	HANDLE( !strcmp, restURI, method, handler )

static struct mg_server * server;

static volatile int s_signal_received = 0;

//inter thread communication pipe between the idle observer and the main thread
int mpdObserverPipe[2];

static const char *mpd_statuses[] = { "UNKNOWN", "STOP", "PLAY", "PAUSE" };
static const char *mpd_entity_types[] = { "UNKNOWN", "DIR", "SONG", "LIST" };

static struct mpd_connection* get_new_mpd_connection( void );
static bool release_mpd_connection( struct mpd_connection *connection );
static void handle_mpd_error( struct mg_connection* mongooseConn, struct mpd_connection *connection );

static void handle_mpd_status( struct mg_connection *conn );
static void handle_mpd_db_entries( struct mg_connection *conn );
static void handle_mpd_send_playlist( struct mg_connection *conn );
static void handle_mpd_send_playlists( struct mg_connection *conn );
static void handle_mpd_play_song( struct mg_connection *conn );
static void handle_mpd_add_song( struct mg_connection *conn );
static void handle_mpd_state( struct mg_connection *conn );
static int event_handler( struct mg_connection *conn, enum mg_event ev );
static void send_404( struct mg_connection *conn, const char *message );

static void signal_handler(int sig_num);

//mpd connection and callbacks
//void setup_mpd_observer( void );
void setup_mpd_observer( int producerPipeFD );
void close_mpd_observer( void );
void radio_mpd_status_changed( void );


const char *getLastSegmentFromURI( const char *uri );
int get_last_segment( const char *uri, char *buf, size_t buf_size );

static bool parse_int(const char *str, int *ret);
static int get_boolean(const char *arg);

int main( void )
{
	config_load( CFG_NAME );

	if( pipe2( mpdObserverPipe, O_NONBLOCK ) != 0 )
	{
		perror( "Could not create pipe" );
		exit( 1 );
	}

	setup_mpd_observer( mpdObserverPipe[ 1 ] );

	server = mg_create_server( NULL, event_handler );
	mg_set_option( server, "listening_port", config_get_string( "ServerPort", "1976" ) );
	mg_set_option( server, "document_root", STATIC_DIR );

	printf( "%s : Server started! Listening on port: %s\n", getTimestampString(), mg_get_option( server, "listening_port" ) );

	signal( SIGTERM, signal_handler );
	signal( SIGINT, signal_handler );

	while( s_signal_received == 0 )
	{
		radio_mpd_status_changed();
		mg_poll_server( server, 100 );
	}
	printf( "Main thread signal: %d\n", s_signal_received );

	close_mpd_observer();
	config_free();
	mg_destroy_server( &server );
	return 1;
}

static void signal_handler( int sig_num )
{
	printf( "Signal received: %d\n", sig_num );
	signal( sig_num, signal_handler ); // Reinstantiate signal handler
	s_signal_received = sig_num;
}

static void send_404( struct mg_connection *conn, const char *message )
{
	mg_send_status( conn, 404 );
	mg_printf_data( conn, "RADIOSERVER : %s", message );
}

static struct mpd_connection* get_new_mpd_connection( void )
{
	const char *mpdServer = config_get_string( "MPDServer", "666" );
	const char *mpdPort = config_get_string( "MPDPort", "666" );

	struct mpd_connection *mpdServerConnection = mpd_connection_new( mpdServer, atoi( mpdPort ), 2000 ); //2csec timeout
	if( mpdServerConnection == NULL )
	{
	}
	else if( mpd_connection_get_error( mpdServerConnection ) )
	{
		mpd_connection_free( mpdServerConnection );
		mpdServerConnection = NULL;
	}
	return mpdServerConnection;
}

static bool release_mpd_connection( struct mpd_connection *connection )
{
	if( connection )
	{
		mpd_connection_free( connection );
	}
	return true;
}

static void handle_mpd_error( struct mg_connection *mongooseConn, struct mpd_connection *connection )
{
	if( connection && mpd_connection_get_error( connection ) != MPD_ERROR_SUCCESS )
	{
		send_404( mongooseConn, mpd_connection_get_error_message( connection ) );
	}
	else
	{
		send_404( mongooseConn, "Error communicating with mpd server" );
	}
}

static void send_access_control_headers( struct mg_connection *conn )
{
	mg_send_status( conn, 204 );
    mg_send_header( conn, "Access-Control-Allow-Origin", "*" );
    mg_send_header( conn, "Access-Control-Max-Age", "1000" );
    mg_send_header( conn, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS" );
    mg_send_header( conn, "Access-Control-Allow-Headers", "X-Requested-With, Content-Type, Origin, Accept, X-Http-Method-Override" );
	mg_send_data( conn, "", 0 );
}

static void send_mpd_json( struct mg_connection *conn, json_t *json )
{
	size_t flags = 0;
	char *json_buffer = json_dumps( json, flags );

	mg_send_header( conn, "Content-Type", "application/json" );

	mg_send_header( conn, "Access-Control-Allow-Origin", "*" );
	mg_send_header( conn, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS" );
	mg_send_header( conn, "Access-Control-Allow-Headers", "Content-Type" );

	mg_send_data( conn, json_buffer, strlen( json_buffer ) );

	free( json_buffer );
}

static json_t *get_mpd_song_as_json( const struct mpd_song *song )
{
	json_t *json_song = json_object();
	json_object_set_new( json_song, "stream", json_string( mpd_song_get_tag( song, MPD_TAG_NAME, 0 ) ?: "" ) );
	json_object_set_new( json_song, "title", json_string( mpd_song_get_tag( song, MPD_TAG_TITLE, 0 ) ?: "" ) );
	json_object_set_new( json_song, "artist", json_string( mpd_song_get_tag( song, MPD_TAG_ARTIST, 0 ) ?: "" ) );
	json_object_set_new( json_song, "album", json_string( mpd_song_get_tag( song, MPD_TAG_ALBUM, 0 ) ?: "" ) );
	json_object_set_new( json_song, "uri", json_string( mpd_song_get_uri( song ) ?: "" ) );
	json_object_set_new( json_song, "duration", json_integer( mpd_song_get_duration( song ) ) );
	json_object_set_new( json_song, "id", json_integer( mpd_song_get_id( song ) ) );
	json_object_set_new( json_song, "pos", json_integer( mpd_song_get_pos( song ) ) );
	return json_song;
}

static void handle_mpd_status( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		if ( !mpd_command_list_begin( mpdServerConnection, true ) ||
			 !mpd_send_status( mpdServerConnection ) ||
			 !mpd_send_current_song( mpdServerConnection ) ||
			 !mpd_command_list_end( mpdServerConnection ) )
		{
			error = true;
			errorMessage = mpd_connection_get_error_message( mpdServerConnection );
		}
		else
		{
			//block and read the server response about the status
			struct mpd_status *status = mpd_recv_status( mpdServerConnection );
			if( !status || mpd_connection_get_error( mpdServerConnection ) != MPD_ERROR_SUCCESS )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			else
			{
				mpd_response_next( mpdServerConnection );
				struct mpd_song *song = mpd_recv_song( mpdServerConnection );
				//there is a possibility that the player is stopped and no current song is available
				if( mpd_connection_get_error( mpdServerConnection ) != MPD_ERROR_SUCCESS || !song )
				{
					json_object_set_new( json, "song", json_boolean( false ) );
				}
				else
				{
					json_object_set_new( json, "song", get_mpd_song_as_json( song ) );
					mpd_song_free( song );
				}

				json_object_set_new( json, "elapsedtime", json_integer( mpd_status_get_elapsed_time( status ) ) );
				json_object_set_new( json, "totaltime", json_integer( mpd_status_get_total_time( status ) ) );
				json_object_set_new( json, "volume", json_integer( mpd_status_get_volume( status ) ) );
				json_object_set_new( json, "state", json_string( mpd_statuses[ mpd_status_get_state( status ) ] ) );
			}

			if( status )
			{
				mpd_status_free( status );
			}
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );

		json_object_clear( json );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}


static void handle_mpd_db_entries( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char path[ 500 ] = "";

		if( mg_get_var( conn, "path", path, sizeof( path ) ) >= 0 )
		{
			if( !mpd_send_list_meta( mpdServerConnection, path ) )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			else
			{
				json_t *json_entities_array = json_array();

				struct mpd_entity *entity;
				enum mpd_entity_type type;
				while( ( entity = mpd_recv_entity( mpdServerConnection ) ) != NULL )
				{
					json_t *json_entity;
					type = mpd_entity_get_type( entity );
					switch( type )
					{
						case MPD_ENTITY_TYPE_DIRECTORY:

							json_entity = json_object();
							json_object_set_new( json_entity, "type", json_string( mpd_entity_types[ type ] ) );
							const struct mpd_directory *directory = mpd_entity_get_directory( entity );
							json_object_set_new( json_entity, "path", json_string( mpd_directory_get_path( directory ) ) );

							break;

						case MPD_ENTITY_TYPE_PLAYLIST:

							json_entity = json_object();
							json_object_set_new( json_entity, "type", json_string( mpd_entity_types[ type ] ) );
							const struct mpd_playlist *playlist = mpd_entity_get_playlist( entity );
							json_object_set_new( json_entity, "title", json_string( mpd_playlist_get_path( playlist ) ) );

							break;

						case MPD_ENTITY_TYPE_SONG: ;

							const struct mpd_song *song = mpd_entity_get_song( entity );
							json_entity = get_mpd_song_as_json( song );
							json_object_set_new( json_entity, "type", json_string( mpd_entity_types[ type ] ) );

							break;

						default:
							break;
					}

					json_array_append_new( json_entities_array, json_entity );
					mpd_entity_free( entity );
				}

				if( json_array_size( json_entities_array ) )
				{
					json_object_set_new( json, "items", json_entities_array );
				}
				else
				{
					json_decref( json_entities_array );
				}
			}

			json_object_set_new( json, "path", json_string( path ) );
		}
		else
		{
			error = true;
			errorMessage = "Invalid path";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		json_object_clear( json );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_send_playlist( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		if( !mpd_send_list_queue_meta( mpdServerConnection ) )
		{
			error = true;
			errorMessage = mpd_connection_get_error_message( mpdServerConnection );
		}
		else
		{
			json_t *json_songs_array = json_array();
			json_t *json_song;
			struct mpd_song *song;
			while( ( song = mpd_recv_song( mpdServerConnection ) ) != NULL )
			{
				json_song = get_mpd_song_as_json( song );
				json_array_append_new( json_songs_array, json_song );
				mpd_song_free( song );
			}

			if( json_array_size( json_songs_array ) )
			{
				json_object_set_new( json, "items", json_songs_array );
			}
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		json_object_clear( json );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_save_playlist( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char playlistVar[ 300 ];
		const char *playlistName;

		if( mg_get_var( conn, "name", playlistVar, sizeof( playlistVar ) ) >= 0 )
		{
			playlistName = playlistVar;
		}
		else
		{
			playlistName = getLastSegmentFromURI( conn->uri );
		}

		if( playlistName )
		{
			if( !mpd_run_save( mpdServerConnection, playlistName ) )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			json_object_set_new( json, "name", json_string( playlistName ) );
		}
		else
		{
			error = true;
			errorMessage = "Invalid playlist name";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		//clear, according to docs will decref to the referenced objects
		//json_object_clear( json_song );
		json_object_clear( json );
		//json_decref( json_song );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_remove_playlist( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char playlistVar[ 300 ];
		const char *playlistName;

		if( mg_get_var( conn, "name", playlistVar, sizeof( playlistVar ) ) >= 0 )
		{
			playlistName = playlistVar;
		}
		else
		{
			playlistName = getLastSegmentFromURI( conn->uri );
		}

		if( playlistName )
		{
			if( !mpd_run_rm( mpdServerConnection, playlistName ) )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			json_object_set_new( json, "name", json_string( playlistName ) );
		}
		else
		{
			error = true;
			errorMessage = "Invalid playlist name";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		//clear, according to docs will decref to the referenced objects
		//json_object_clear( json_song );
		json_object_clear( json );
		//json_decref( json_song );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_send_playlists( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		if( !mpd_send_list_playlists( mpdServerConnection ) )
		{
			error = true;
			errorMessage = mpd_connection_get_error_message( mpdServerConnection );
		}
		else
		{
			json_t *json_playlists_array = json_array();
			struct mpd_playlist *playlist;
			while( ( playlist = mpd_recv_playlist( mpdServerConnection ) ) != NULL )
			{
				json_t *json_playlist = json_object();
				json_object_set_new( json_playlist, "title", json_string( mpd_playlist_get_path( playlist ) ) );
				json_array_append_new( json_playlists_array, json_playlist );
				mpd_playlist_free( playlist );
			}

			if( json_array_size( json_playlists_array ) )
			{
				json_object_set_new( json, "items", json_playlists_array );
			}
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		//clear, according to docs will decref to the referenced objects
		//json_object_clear( json_song );
		json_object_clear( json );
		//json_decref( json_song );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}


static void handle_mpd_load_playlists( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		const char *playlistName = getLastSegmentFromURI( conn->uri );

		if( playlistName )
		{
			if( !mpd_run_load( mpdServerConnection, playlistName ) )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			else
			{
				json_t *json_playlists_array = json_array();
				struct mpd_playlist *playlist;
				while( ( playlist = mpd_recv_playlist( mpdServerConnection ) ) != NULL )
				{
					json_t *json_playlist = json_object();
					json_object_set_new( json_playlist, "title", json_string( mpd_playlist_get_path( playlist ) ) );
					json_array_append_new( json_playlists_array, json_playlist );
					mpd_playlist_free( playlist );
				}

				if( json_array_size( json_playlists_array ) )
				{
					json_object_set_new( json, "items", json_playlists_array );
				}
			}
		}
		else
		{
			error = true;
			errorMessage = "Incorrect playlist name";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );

		//clear, according to docs will decref to the referenced objects
		//json_object_clear( json_song );
		json_object_clear( json );
		//json_decref( json_song );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}


static void handle_mpd_play_song( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char songId[ 10 ];

		if( mg_get_var( conn, "id", songId, sizeof( songId ) ) >= 0 )
		{
			if( !mpd_run_play_id( mpdServerConnection, atoi( songId ) ) )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			json_object_set_new( json, "songid", json_string( songId ) );
		}
		else
		{
			error = true;
			errorMessage = "Incorrect playlist name";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		//clear, according to docs will decref to the referenced objects
		//json_object_clear( json_song );
		json_object_clear( json );
		//json_decref( json_song );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_remove_song( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		//char songPos[ 10 ];
		int songId;
		const char *songIdSegment = getLastSegmentFromURI( conn->uri );
		if( songIdSegment )
		{
			if( parse_int( songIdSegment, &songId ) )
			{
				if( !mpd_run_delete( mpdServerConnection, songId ) )
				{
					error = true;
					errorMessage = mpd_connection_get_error_message( mpdServerConnection );
				}
			}
			else if( !strcmp( songIdSegment, "all" ) )
			{
				if( !mpd_run_clear( mpdServerConnection ) )
				{
					error = true;
					errorMessage = mpd_connection_get_error_message( mpdServerConnection );
				}

			}
			else
			{
				error = true;
				errorMessage = "Can delete either a song id or all songs";
			}
			json_object_set_new( json, "songid", json_string( songIdSegment ) );
		}
		else
		{
			error = true;
			errorMessage = "Incorrect song id";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		//clear, according to docs will decref to the referenced objects
		//json_object_clear( json_song );
		json_object_clear( json );
		//json_decref( json_song );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_add_song( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char songId[ 10 ];
		char songURI[ 300 ];
		int newId;

		if( mg_get_var( conn, "uri", songURI, sizeof( songURI ) ) >= 0 )
		{
			if( !mpd_run_add( mpdServerConnection, songURI ) )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			json_object_set_new( json, "uri", json_string( songURI ) );
		}
		else if( mg_get_var( conn, "id", songId, sizeof( songId ) ) >= 0 )
		{
			newId = mpd_run_add_id( mpdServerConnection, songId );
			if( newId < 0  )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			json_object_set_new( json, "id", json_string( songId ) );
			json_object_set_new( json, "newid", json_integer( newId ) );
		}
		else
		{
			error = true;
			errorMessage = "Incorrect song uri or id";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		//clear, according to docs will decref to the referenced objects
		//json_object_clear( json_song );
		json_object_clear( json );
		//json_decref( json_song );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_search_song( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char searchString[ 300 ];

		if( mg_get_var( conn, "search", searchString, sizeof( searchString ) ) >= 0 )
		{
			mpd_search_db_songs( mpdServerConnection, false );
			if( !mpd_search_add_any_tag_constraint( mpdServerConnection, MPD_OPERATOR_DEFAULT, searchString ) || !mpd_search_commit( mpdServerConnection ) )
			{
				error = true;
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			else
			{
				json_t *json_songs_array = json_array();
				json_t *json_song;
				struct mpd_song *song;
				while( ( song = mpd_recv_song( mpdServerConnection ) ) != NULL )
				{
					json_song = get_mpd_song_as_json( song );
					json_object_set_new( json_song, "type", json_string( mpd_entity_types[ MPD_ENTITY_TYPE_SONG ] ) );
					json_array_append_new( json_songs_array, json_song );
					mpd_song_free( song );
				}

				if( json_array_size( json_songs_array ) )
				{
					json_object_set_new( json, "items", json_songs_array );
				}
			}
			json_object_set_new( json, "search", json_string( searchString ) );
		}
		else
		{
			error = true;
			errorMessage = "Invalid search string";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		json_object_clear( json );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_state( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char stateName[ 20 ];

		if( mg_get_var( conn, "state", stateName, sizeof( stateName ) ) >= 0 )
		{
			if( !strcasecmp( stateName, "pause" ) )
			{
				if( !mpd_run_toggle_pause( mpdServerConnection ) )
				{
					error = true;
				}
			}
			else if( !strcasecmp( stateName, "play" ) )
			{
				if( !mpd_run_play( mpdServerConnection ) )
				{
					error = true;
				}
			}
			else if( !strcasecmp( stateName, "stop" ) )
			{
				if( !mpd_run_stop( mpdServerConnection ) )
				{
					error = true;
				}
			}
			else if( !strcasecmp( stateName, "prev" ) || !strcasecmp( stateName, "previous" ) )
			{
				if( !mpd_run_previous( mpdServerConnection ) )
				{
					error = true;
				}
			}
			else if( !strcasecmp( stateName, "next" ) )
			{
				if( !mpd_run_next( mpdServerConnection ) )
				{
					error = true;
				}
			}
			if( error )
			{
				errorMessage = mpd_connection_get_error_message( mpdServerConnection );
			}
			json_object_set_new( json, "state", json_string( stateName ) );
		}
		else
		{
			error = true;
			errorMessage = "Invalid state variable";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		json_object_clear( json );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static void handle_mpd_volume( struct mg_connection *conn )
{
	struct mpd_connection *mpdServerConnection = get_new_mpd_connection();

	if( mpdServerConnection )
	{
		bool error = false;
		const char *errorMessage;

		json_t *json = json_object();

		char volume[ 10 ];
		int volumeInt;

		if( mg_get_var( conn, "value", volume, sizeof( volume ) ) >= 0 )
		{
			if( parse_int( volume, &volumeInt ) )
			{
				if( !mpd_run_set_volume( mpdServerConnection, volumeInt ) )
				{
					error = true;
					errorMessage = mpd_connection_get_error_message( mpdServerConnection );
				}
			}
			else
			{
				error = true;
				errorMessage = "Volume should be an integer";
			}
			json_object_set_new( json, "volume", json_string( volume ) );
		}
		else
		{
			error = true;
			errorMessage = "Invalid volume";
		}

		if( error )
		{
			json_object_set_new( json, "message", json_string( errorMessage ) );
		}
		json_object_set_new( json, "success", json_boolean( !error ) );

		send_mpd_json( conn, json );
		json_object_clear( json );
		json_decref( json );
	}
	else
	{
		handle_mpd_error( conn, mpdServerConnection );
	}

	release_mpd_connection( mpdServerConnection );
}

static int event_handler( struct mg_connection *conn, enum mg_event ev )
{
	switch( ev )
	{
		case MG_AUTH:
			return MG_TRUE;
		case MG_REQUEST:
			//websocket is handled automatically. the client is subscribed and will be sent an idle event
			if( conn->is_websocket )
			{
				return MG_TRUE;
			}
			//confirm accepting cross domain requests
			else if( !strcmp( conn->request_method, "OPTIONS" ) )
			{
				send_access_control_headers( conn );
				return MG_TRUE;
			}
			//Status actions
			HANDLE_EXACT( "/api/mpd",				"GET",		handle_mpd_status )
			HANDLE_EXACT( "/api/mpd/status",		"GET",		handle_mpd_status )
			//Playlist actions
			HANDLE_EXACT( "/api/mpd/playlist",		"GET",		handle_mpd_send_playlist )
			HANDLE_START( "/api/mpd/playlist/",		"POST",		handle_mpd_save_playlist )
			//Saved playlists actions
			HANDLE_EXACT( "/api/mpd/playlists",		"GET",		handle_mpd_send_playlists )
			HANDLE_START( "/api/mpd/playlists/",	"GET",		handle_mpd_load_playlists )
			HANDLE_START( "/api/mpd/playlists/",	"DELETE",	handle_mpd_remove_playlist )
			//Song actions
			HANDLE_EXACT( "/api/mpd/play",			"POST",		handle_mpd_play_song )
			HANDLE_EXACT( "/api/mpd/song",			"POST",		handle_mpd_add_song )
			HANDLE_START( "/api/mpd/song/",			"DELETE",	handle_mpd_remove_song )
			//change state
			HANDLE_EXACT( "/api/mpd/state",			"POST",		handle_mpd_state )
			//change volume
			HANDLE_EXACT( "/api/mpd/volume",		"POST",		handle_mpd_volume )
			//search for songs
			HANDLE_EXACT( "/api/mpd/search",		"GET",		handle_mpd_search_song )
			HANDLE_EXACT( "/api/mpd/search",		"POST",		handle_mpd_search_song )
			//browse for songs
			HANDLE_EXACT( "/api/mpd/browse",		"GET",		handle_mpd_db_entries )
			//otherwise
			//serve static files
			return MG_FALSE;
		case MG_CLOSE:
			return MG_TRUE;
		default:
			return MG_FALSE;
	}
}


char* const idleEventDisconnect = "DISCONNECT";
char* const idleEventConnect = "CONNECT";

//MPD support functions
void radio_mpd_status_changed( void )
{
	int state;
	int readRes = read( mpdObserverPipe[ 0 ], &state, sizeof(state) );
	if( readRes<= 0 )
	{
		return;
	}

	enum mpd_idle what = state;
	const char* idleEventString;

	if( what == MPD_CLIENT_DISCONNECT )
	{
		idleEventString = idleEventDisconnect;
	}
	else if( what == MPD_CLIENT_CONNECT )
	{
		idleEventString = idleEventConnect;
	}
	else
	{
		idleEventString = mpd_idle_name( what );
	}

	printf( "%s : MPD IDLE event %s\n", getTimestampString(), idleEventString );

	//notify websocket clients
	struct mg_connection *ws_conn;
	int clientCount = 0;
	for( ws_conn = mg_next( server, NULL ); ws_conn != NULL; ws_conn = mg_next( server, ws_conn ) )
	{
		if( !ws_conn->is_websocket )
		{
			continue;
		}
		clientCount++;
		mg_websocket_printf( ws_conn, WEBSOCKET_OPCODE_TEXT, idleEventString );
	}
	printf( "Notified %d websocket client(s)\n", clientCount );
}

void setup_mpd_observer( int producerPipeFD )
{
	const char *mpdPort = config_get_string( "MPDPort", "6600" );
	const char *mpdServer = config_get_string( "MPDServer", "localhost" );

	radio_mpd_connect( mpdServer, mpdPort, producerPipeFD );
}

void close_mpd_observer( void )
{
	radio_mpd_close();
}

//misc functions
static bool parse_int(const char *str, int *ret)
{
	char *test;
	int temp = strtol(str, &test, 10);

	if (*test != '\0')
		return false; /* failure */

	*ret = temp;
	return true; /* success */
}

static int get_boolean(const char *arg)
{
	static const struct _bool_table {
		const char * on;
		const char * off;
	} bool_table [] = {
		{ "on", "off" },
		{ "1", "0" },
		{ "true", "false" },
		{ "yes", "no" },
		{ .on = NULL }
	};

	unsigned i;
	for (i = 0; bool_table[i].on != NULL; ++i) {
		if (strcasecmp(arg,bool_table[i].on) == 0)
			return 1;
		else if (strcasecmp(arg,bool_table[i].off) == 0)
			return 0;
	}
	return -1;
}

const char *getLastSegmentFromURI( const char *uri )
{
	char *lastSegment = strrchr( uri, '/' );
	if( lastSegment )
	{
		lastSegment++;
	}
	return lastSegment;
}


int get_last_segment( const char *uri, char *buf, size_t buf_size )
{
	char *lastSegment = strrchr( uri, '/' );
	if( lastSegment )
	{
		int len = strlen( lastSegment );
		lastSegment++;
		if( len > buf_size-1 )
		{
			return -2;
		}
		else
		{
			strncpy( buf, lastSegment, buf_size - 1 );
			return len;
		}
	}
	else
	{
		return -1;
	}
}
