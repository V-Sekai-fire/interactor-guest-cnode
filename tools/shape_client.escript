%% round trip against guest@127.0.0.1: ping, info, vmcall with nothing loaded (must be an error), unknown command (must be an error)
main(_) ->
    {ok, _} = net_kernel:start(['client@127.0.0.1', longnames]),
    erlang:set_cookie(node(), 'deckcookie'),
    Target = {guest, 'guest@127.0.0.1'},
    Ask = fun(Cmd, Arg) ->
        Ref = make_ref(), Target ! {Cmd, Ref, Arg},
        receive {Ref, R} -> R after 5000 -> timeout end
    end,
    io:format("ping    ~p~n", [Ask(ping, nil)]),
    io:format("info    ~p~n", [Ask(info, nil)]),
    io:format("vmcall  ~p~n", [Ask(vmcall, {<<"nothing">>, []})]),
    io:format("unknown ~p~n", [Ask(frobnicate, nil)]),
    io:format("cancel  ~p~n", [Ask(cancel, nil)]).
