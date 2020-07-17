define(
    function()
    {
        var CONFIG = {
            DOMAIN : "radio.kafka.linux-bg.org",
            EP_STATUS : "/api/mpd/status",
            EP_PLAYLIST : "/api/mpd/playlist",
            EP_PLAYLISTS : "/api/mpd/playlists",
            EP_PLAY : "/api/mpd/play",
            get WS_ADDRESS ()
            {
                return "ws://" + this.DOMAIN + "/ws/"
            },
            init : function()
            {
                for( var prop in this )
                {
                    if( prop.indexOf( 'EP_' ) == 0 )
                    {
                        this[ prop ] = "http://" + this.DOMAIN + this[ prop ];
                    }
                }
                return this;
            }
        }.init();

        return CONFIG
    }
);
