define(
    [ 'jquery', 'knockout', 'komapping' ],
    function( $, ko, komap )
    {
        var MPDSong = function( songData )
        {
            if( songData )
            {
                komap.fromJS( songData, {}, this );
            }

            var song = this;

            this.isActive = ko.observable( false );
            this.isSelected = ko.observable( false );

            this.isRadio = ko.computed(
                function()
                {
                    if( this.uri().indexOf( 'http' ) == 0 )
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                },
                song,
                {
                    deferEvaluation : true
                }
            );

            this.displayName = ko.computed(
                function()
                {
                    var displayName = '';
                    if( this.isRadio() )
                    {
                        //this is a radio
                        if( this.isActive() )
                        {
                            displayName = this.title() || this.stream() || this.uri();
                        }
                        else
                        {
                            displayName = this.stream() || this.uri();
                        }
                    }
                    else
                    {
                        if( this.title().length > 0 )
                        {
                            displayName = this.title();
                        }
                        else
                        {
                            displayName = this.uri().split("/").pop().split( "." )[0];
                        }
                    }

                    return displayName;
                },
                song,
                {
                    deferEvaluation : true
                }
            );
        }

        var MPDSongsMapping = {
            key : function( item )
            {
                return ko.utils.unwrapObservable( item.id );
            },
            create : function( options )
            {
                return new MPDSong( options.data );
            }
        };

        function MPDSongs( url )
        {
            var self = this;
            this.url = url || '';
            this.songs = ko.observableArray([]);
            this.load = function( url )
            {
                this.url = url || this.url;
                if( this.url == '' )
                {
                    throw 'invalid songs url';
                }
                var self = this;
                $.ajax(
                    {
                        url : this.url,
                        success : function( result )
                        {
                            komap.fromJS( result.items, MPDSongsMapping, self.songs );
                        }
                    }
                );
            };
        }

        return MPDSongs;
    }
)
