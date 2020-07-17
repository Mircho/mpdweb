define(
    [ 'knockout', 'text!./list.html' ],
    function( ko, htmlString )
    {
        return {
            viewModel : {
                                createViewModel :   function( params, componentInfo )
                                                    {
                                                        var listInstance = params.list;
                                                        //add selectMode observable to track the state of the list editing
                                                        if( typeof listInstance[ 'selectMode' ] === 'undefined' )
                                                        {
                                                            listInstance.selectMode = ko.observable( false );
                                                        }
                                                        return listInstance;
                                                    }
            },
            template :          htmlString
        }
    }
);
