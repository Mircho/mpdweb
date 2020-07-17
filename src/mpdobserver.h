#ifdef __cplusplus
extern "C" {
#endif

#include <mpd/idle.h>

#ifndef __MPDOBSERVER_H__
#define __MPDOBSERVER_H__

//we will stuff those in the callback as mpd_idle
#define MPD_CLIENT_DISCONNECT 11
#define MPD_CLIENT_CONNECT 12

typedef void (*MPDStatusChanged)( enum mpd_idle changeType );

void radio_mpd_connect( const char *server, const char *port, int pipeFD );
void radio_mpd_close( void );
void radio_force_refresh( void );

#endif //__MPDOBSERVER_H__

#ifdef __cplusplus
}
#endif
