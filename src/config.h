#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <uci.h>

#define RADIOSERVER_SECTION "radioserver"

#define CONFIG_ERROR_CONTEXT_LOAD_FAILED	1
#define CONFIG_ERROR_FILE_LOAD_FAILED		2
#define CONFIG_ERROR_SECTION_NOT_FOUND		3

#ifndef __CONFIG_H__
#define __CONFIG_H__

uint8_t config_load( const char *cfgfile );
void config_free( void );
const char* config_get_string( char *optionname, const char *defaultvalue );

#endif //__CONFIG_H__

#ifdef __cplusplus
}
#endif
