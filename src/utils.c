#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils.h"

char* const getTimestampString( void )
{
	time_t timeVal;
	struct tm localTime;
	static char timeBuffer[ 10 ];

	timeVal = time( NULL );
	memcpy( &localTime, localtime( &timeVal ), sizeof( localTime ) );

	strftime( timeBuffer, 10, "%H:%M:%S", &localTime );

	return timeBuffer;
}
