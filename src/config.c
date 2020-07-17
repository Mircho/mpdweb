#include <string.h>
#include "config.h"

/*
 * reference : https://gitlab.labs.nic.cz/turris/ucollect/raw/b5db98ce5aa49dacea95635c56098a9820fec9aa/src/core/configure.c
 *
 *
 */

#define LOCAL_CONFIG_DIR "./config/"

static uint8_t bInitialized = 0;
static struct uci_context *uciContext;
static struct uci_package *package;
static struct uci_section *section;

uint8_t config_load( const char *cfgfile )
{
	uciContext = uci_alloc_context();
	if( !uciContext )
	{
		uci_perror( uciContext, "Failed to allocate config context" );
		return CONFIG_ERROR_CONTEXT_LOAD_FAILED;
	}

#if defined(LOCAL_CONFIG) & LOCAL_CONFIG==1
	uci_set_confdir( uciContext, LOCAL_CONFIG_DIR );
#endif

	int success = uci_load( uciContext, cfgfile, &package );
	if( success != UCI_OK || !package )
	{
		uci_perror( uciContext, "Failed to load config file" );
		return CONFIG_ERROR_FILE_LOAD_FAILED;
	}

	struct uci_element *itersect;
	uci_foreach_element( &package->sections, itersect )
	{
		struct uci_section *s = uci_to_section( itersect );
		if( strcmp( s->type, RADIOSERVER_SECTION ) == 0 )
		{
			section = s;
		}
	}
	//for some reason does not work
	//section = uci_lookup_section( uciContext, package, RADIOSERVER_SECTION );

	if( !section )
	{
		uci_perror( uciContext, "Failed to find section" );
		return CONFIG_ERROR_SECTION_NOT_FOUND;
	}

	bInitialized = 1;
	return 0;
}


void config_free( void )
{
	if( package && uciContext )
	{
		uci_unload( uciContext, package );
	}
	if( uciContext )
	{
		uci_free_context( uciContext );
	}
}


const char* config_get_string( char *optionname, const char *defaultvalue )
{
	if( !bInitialized )
	{
		printf( "Not initialized!\n" );
		return NULL;
	}

	const char *result = uci_lookup_option_string( uciContext, section, optionname );
	return result==NULL ? defaultvalue : result;
}
