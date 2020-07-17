define(
    [ "jquery", "knockout" ],
    function( $, ko )
    {
        /**
         * Original article
         * http://aboutcode.net/2012/11/15/twitter-bootstrap-modals-and-knockoutjs.html
         **/
        function showModal( options )
        {
            var viewModel = options.viewModel;
            var template = options.template || viewModel.template;
            var context = options.context;
            return __createModalElement( template, viewModel )
                    .pipe( $ )
                    .pipe(
                            function( $ui )
                            {
                                var deferredModalResult = $.Deferred();
                                viewModel.modal = {
                                    close : function( result )
                                            {
                                                if( typeof result != 'undefined' )
                                                {
                                                    deferredModalResult.resolveWith( context, [result] )
                                                }
                                                else
                                                {
                                                    deferredModalResult.rejectWith( context, [] );
                                                }
                                            }
                                };
                                $ui.modal();
                                $ui.on( 'hidden',
                                    function()
                                    {
                                        $ui.each(
                                            function ( index, element )
                                            {
                                                ko.cleanNode( element );
                                            }
                                        );
                                        $ui.remove();
                                    }
                                );
                                deferredModalResult.always(
                                    function()
                                    {
                                        $ui.modal( 'hide' );
                                    }
                                );
                                return deferredModalResult;
                            }
                         );

        }
        function __createModalElement( templateName, viewModel )
        {
            var tempDiv = document.createElement( 'DIV' );
            tempDiv.style.display = 'none';
            document.body.appendChild( tempDiv );

            var deferredElement = $.Deferred();

            ko.renderTemplate(
                templateName,
                viewModel,
                {
                    afterRender :   function( nodes )
                                    {
                                        var elements = nodes.filter(
                                            function( node )
                                            {
                                                return node.nodeType === 1;
                                            }
                                        );
                                        deferredElement.resolve( elements[ 0 ] );
                                    }
                },
                tempDiv,
                "replaceNode"
            );

            return deferredElement;
        }

        function initToggle()
        {
            $( "#menu-toggle" ).click(
                function(e)
                {
                    e.preventDefault();
                    $( "#wrapper" ).toggleClass( "toggled" );
                }
            );
        }

        return {
            initToggle : initToggle,
            showModal : showModal
        }
    }
);
