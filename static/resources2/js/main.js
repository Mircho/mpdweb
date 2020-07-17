requirejs.config({
    shim : {
        "bootstrap" : { "deps" : [ 'jquery' ] },
    },
    baseUrl: 'resources2/js/lib',
    paths: {
        text        : 'text',
        jquery      : 'jquery-2.1.4',
        knockout    : 'knockout-3.3.0.debug',
        komapping   : 'knockout.mapping-latest',
        knockstrap  : 'knockstrap',
        bootstrap   : '../../bootstrap/js/bootstrap',
        app         : '../app',
        CONFIG      : '../app/config',
        ui          : '../app/ui',
        songlist    : '../app/songlist',
        list        : '../app/list',
    },
    urlArgs : "bust=" + Math.random()
});

define(
    function( require )
    {
        var $ = require( 'jquery' ),
            ko = require( 'knockout' ),
            bootstrap = require( 'bootstrap' ),
            songlist = require( 'songlist' ),
            ui = require( 'ui' ),
            CONFIG = require( 'CONFIG' );

        var ModalVM = function( options )
        {
            this.template = options.template || '';
            this.data = options.data || null;
        }

        ModalVM.prototype.cancel = function()
        {
            this.modal.close();
        }

        ModalVM.prototype.action = function()
        {
            this.modal.close( this.data );
        }

        var slist = new songlist();
        $.extend(
            slist,
            {
                //selectMode : ko.observable( false ),
                selectedSongs : ko.computed(
                    function()
                    {
                        return ko.utils.arrayFilter(
                            slist.songs(),
                            function( song )
                            {
                                return song.isSelected();
                            }
                        );
                    }
                ),
                onDelete : function()
                {
                    var self = this;

                    var deleteVM = new ModalVM(
                        {
                            data: {
                                songs: self.selectedSongs,
                            }
                        }
                    );
                    ui.showModal(
                        {
                            template : 'mpdradioModalTemplateRemoveSong',
                            viewModel : deleteVM,
                            context : self
                        }
                    ).done(
                        function()
                        {
                            console.log();
                            // jQuery.ajax(
                            //     {
                            //         url : REMOVE_SONG_ADDRESS + '/' + song.pos(),
                            //         type : 'DELETE'
                            //     }
                            // );
                        }
                    ).fail(
                        function()
                        {

                        }
                    );
                },
                onSelect : function( song )
                {
                    if( this.selectMode() )
                    {
                        song.isSelected( !song.isSelected() );
                        return;
                    }
                    jQuery.post(
                        CONFIG.EP_PLAY,
                        {
                            id : song.id()
                        }
                    );
                }
            }
        );

        var MPDVM = {
            currentPlayList : slist
        }

        ko.components.register( 'list', { require : 'list' } );
        MPDVM.currentPlayList.load( CONFIG.EP_PLAYLIST );

        $(
            function()
            {
                ui.initToggle();
                ko.applyBindings( MPDVM );
            }
        )

    }
)
