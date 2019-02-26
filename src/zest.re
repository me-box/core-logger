open Lwt.Infix;

type t = {
  zmq_ctx: ZMQ.Context.t,
  db: Timeseries.t,
  main_endpoint: string,
  router_endpoint: string,
  path: string,
  key: string,
  token: string,
  max_age: int,
};

let main_public_key = ref("");
let main_secret_key = ref("");
let router_public_key = ref("");
let router_secret_key = ref("");

module Response = {
  type t =
    | OK
    | Unavailable
    | Payload(string)
    | Observe(string, string)
    | Notify(string)
    | Error(string);
};

let create_content_format = id => {
  let bits = [%bitstring {|id : 16 : bigendian|}];
  Bitstring.string_of_bitstring(bits);
};

let handle_header = bits => {
  let tuple =
    switch%bitstring (bits) {
    | {|code : 8 : unsigned;
        oc : 8 : unsigned;
        tkl : 16 : bigendian;
        rest : -1 : bitstring
     |} => (
        tkl,
        oc,
        code,
        rest,
      )
    | {|_|} => failwith("invalid header")
    };
  tuple;
};

let handle_option = bits => {
  let tuple =
    switch%bitstring (bits) {
    | {|number : 16 : bigendian;
        len : 16 : bigendian;
        value: len*8: string;
        rest : -1 : bitstring
      |} => (
        number,
        value,
        rest,
      )
    | {|_|} => failwith("invalid options")
    };
  tuple;
};

let handle_options = (oc, bits) => {
  let options = Array.make(oc, (0, ""));
  let rec handle = (oc, bits) =>
    if (oc == 0) {
      bits;
    } else {
      let (number, value, r) = handle_option(bits);
      options[oc - 1] = (number, value);
      let _ = Lwt_log_core.debug_f("option => %d:%s", number, value);
      handle(oc - 1, r);
    };
  (options, handle(oc, bits));
};

let has_public_key = options =>
  if (Array.exists(((number, _)) => number == 2048, options)) {
    true;
  } else {
    false;
  };

let get_option_value = (options, value) => {
  let rec find = (a, x, i) => {
    let (number, value) = a[i];
    if (number == x) {
      value;
    } else {
      find(a, x, i + 1);
    };
  };
  find(options, value, 0);
};

let handle_ack_content = (options, payload) => {
  let payload = Bitstring.string_of_bitstring(payload);
  if (has_public_key(options)) {
    let key = get_option_value(options, 2048);
    Response.Observe(key, payload) |> Lwt.return;
  } else {
    Response.Payload(payload) |> Lwt.return;
  };
};

let handle_ack_created = options =>
  if (has_public_key(options)) {
    let key = get_option_value(options, 2048);
    Response.Notify(key) |> Lwt.return;
  } else {
    Response.OK |> Lwt.return;
  };

let handle_ack_deleted = options => Response.OK |> Lwt.return;

let handle_service_unavailable = options => Response.Unavailable |> Lwt.return;

let handle_ack_bad_request = options =>
  Response.Error("Bad Request") |> Lwt.return;

let handle_unsupported_content_format = options =>
  Response.Error("Unsupported Content-Format") |> Lwt.return;

let handle_ack_unauthorized = options =>
  Response.Error("Unauthorized") |> Lwt.return;

let handle_not_acceptable = options =>
  Response.Error("Not Acceptable") |> Lwt.return;

let handle_request_entity_too_large = options =>
  Response.Error("Request Entity Too Large") |> Lwt.return;

let handle_internal_server_error = options =>
  Response.Error("Internal Server Error") |> Lwt.return;

let handle_response = msg =>
  Lwt_log_core.debug_f("Received:\n%s", msg)
  >>= (
    () => {
      let r0 = Bitstring.bitstring_of_string(msg);
      let (tkl, oc, code, r1) = handle_header(r0);
      let (options, payload) = handle_options(oc, r1);
      switch (code) {
      | 69 => handle_ack_content(options, payload)
      | 65 => handle_ack_created(options)
      | 66 => handle_ack_deleted(options)
      | 128 => handle_ack_bad_request(options)
      | 129 => handle_ack_unauthorized(options)
      | 143 => handle_unsupported_content_format(options)
      | 163 => handle_service_unavailable(options)
      | 134 => handle_not_acceptable(options)
      | 141 => handle_request_entity_too_large(options)
      | 160 => handle_internal_server_error(options)
      | _ => failwith("invalid code:" ++ string_of_int(code))
      };
    }
  );

let send_request = (~msg, ~to_ as socket) =>
  Lwt_log_core.debug_f("Sending:\n%s", msg)
  >>= (
    () =>
      Lwt_zmq.Socket.send(socket, msg)
      >>= (() => Lwt_zmq.Socket.recv(socket) >>= handle_response)
  );

let create_header = (~tkl, ~oc, ~code) => {
  let bits = [%bitstring
    {|code : 8 : unsigned;
      oc : 8 : unsigned;
      tkl : 16 : bigendian
    |}
  ];
  (bits, 32);
};

let create_option = (~number, ~value) => {
  let byte_length = String.length(value);
  let bit_length = byte_length * 8;
  let bits = [%bitstring
    {|number : 16 : bigendian;
      byte_length : 16 : bigendian;
      value : bit_length : string
    |}
  ];
  (bits, bit_length + 32);
};

let create_token = (~tk as token) => {
  let bit_length = String.length(token) * 8;
  (token, bit_length);
};

let create_options = options => {
  let count = Array.length(options);
  let values = Array.map(((x, y)) => x, options);
  let value = Bitstring.concat(Array.to_list(values));
  let lengths = Array.map(((x, y)) => y, options);
  let length = Array.fold_left((x, y) => x + y, 0, lengths);
  (value, length, count);
};

let create_observe_option_max_age = seconds => {
  let bits = [%bitstring {|seconds : 32 : bigendian|}];
  Bitstring.string_of_bitstring(bits);
};

let create_max_age = seconds => {
  let bits = [%bitstring {|seconds : 32 : bigendian|}];
  Bitstring.string_of_bitstring(bits);
};

let create_observe_options = (~format, ~age, ~uri) => {
  let uri_path = create_option(~number=11, ~value=uri);
  let uri_host = create_option(~number=3, ~value=Unix.gethostname());
  let content_format = create_option(~number=12, ~value=format);
  let observe = create_option(~number=6, ~value="audit");
  let max_age =
    create_option(~number=14, ~value=create_max_age(Int32.of_int(age)));
  create_options([|uri_path, uri_host, observe, content_format, max_age|]);
};

let message = (~token, ~format, ~uri, ~age) => {
  let (options_value, options_length, options_count) =
    create_observe_options(~age, ~uri, ~format);
  let (header_value, header_length) =
    create_header(~tkl=String.length(token), ~oc=options_count, ~code=1);
  let (token_value, token_length) = create_token(~tk=token);
  let bits = [%bitstring
    {|header_value : header_length : bitstring;
      token_value : token_length : string;
      options_value : options_length : bitstring
    |}
  ];
  Bitstring.string_of_bitstring(bits);
};

let set_main_socket_security = (soc, key) => {
  ZMQ.Socket.set_curve_serverkey(soc, key);
  ZMQ.Socket.set_curve_publickey(soc, main_public_key^);
  ZMQ.Socket.set_curve_secretkey(soc, main_secret_key^);
};

let set_dealer_socket_security = (soc, key) => {
  ZMQ.Socket.set_curve_serverkey(soc, key);
  ZMQ.Socket.set_curve_publickey(soc, router_public_key^);
  ZMQ.Socket.set_curve_secretkey(soc, router_secret_key^);
};

let connect_request_socket = (endpoint, ctx, kind) => {
  let soc = ZMQ.Socket.create(ctx.zmq_ctx, kind);
  set_main_socket_security(soc, ctx.key);
  ZMQ.Socket.connect(soc, endpoint);
  Lwt_zmq.Socket.of_socket(soc);
};

let connect_dealer_socket = (key, ident, endpoint, ctx, kind) => {
  let soc = ZMQ.Socket.create(ctx, kind);
  set_dealer_socket_security(soc, key);
  ZMQ.Socket.set_identity(soc, ident);
  ZMQ.Socket.connect(soc, endpoint);
  Lwt_zmq.Socket.of_socket(soc);
};

let close_socket = lwt_soc => {
  let soc = Lwt_zmq.Socket.to_socket(lwt_soc);
  ZMQ.Socket.close(soc);
};

let info = (fmt) => Irmin_unix.info(~author="core logger", fmt);

let write_log = (ctx, id, json) => {
  open Timeseries;
  switch(validate_json(json)) {
  | Some((t,j)) => write(~ctx=ctx.db, ~info=info("write"), ~timestamp=t, ~id=id, ~json=j)
  | None => failwith("Error:badly formatted JSON\n")
  };
}

let observe_loop = (ctx, socket) => {
  let rec loop = () =>
    Lwt_zmq.Socket.recv(socket)
    >>= handle_response
    >>= (
      resp =>
        switch (resp) {
        | Response.Payload(msg) => 
          Lwt_io.printf("%s\n", msg) >>=
            () => write_log(ctx, "foo", Ezjsonm.dict([("value", `String(msg))])) >>=
              () => loop();
        | Response.Unavailable => Lwt_io.printf("=> observation ended\n")
        | _ => failwith("unhandled response")
        }
    );
  loop();
};

let set_socket_subscription = (socket, path) => {
  let soc = Lwt_zmq.Socket.to_socket(socket);
  ZMQ.Socket.subscribe(soc, path);
};

let observe_worker = ctx => {
  let req_soc =
    connect_request_socket(ctx.main_endpoint, ctx, ZMQ.Socket.req);
  Lwt_log_core.debug_f("Subscribing:%s", ctx.path)
  >>= (
    () =>
      send_request(
        ~msg=
          message(
            ~token=ctx.token,
            ~format=create_content_format(50),
            ~uri=ctx.path,
            ~age=ctx.max_age,
          ),
        ~to_=req_soc,
      )
      >>= (
        resp =>
          switch (resp) {
          | Response.Observe(key, ident) =>
            Lwt_log_core.debug_f(
              "Observing:%s with ident:%s",
              ctx.path,
              ident,
            )
            >>= (
              () => {
                close_socket(req_soc);
                let deal_soc =
                  connect_dealer_socket(
                    key,
                    ident,
                    ctx.router_endpoint,
                    ctx.zmq_ctx,
                    ZMQ.Socket.dealer,
                  );
                observe_loop(ctx, deal_soc)
                >>= (() => close_socket(deal_soc) |> Lwt.return);
              }
            )
          | Response.Error(msg) =>
            close_socket(req_soc) |> (() => Lwt_io.printf("=> %s\n", msg))
          | _ => failwith("unhandled response")
          }
      )
  );
};

let setup_main_keys = () => {
  let (public_key, private_key) = ZMQ.Curve.keypair();
  main_public_key := public_key;
  main_secret_key := private_key;
};

let setup_router_keys = () => {
  let (public_key, private_key) = ZMQ.Curve.keypair();
  router_public_key := public_key;
  router_secret_key := private_key;
};

let observe =
    (
      ~db,
      ~main_endpoint="tcp://127.0.0.1:5555",
      ~router_endpoint="tcp://127.0.0.1:5556",
      ~path,
      ~key,
      ~token="",
      ~max_age=0,
      (),
    ) => {
  setup_main_keys();
  setup_router_keys();
  let ctx = {
    zmq_ctx: ZMQ.Context.create(),
    db,
    main_endpoint,
    router_endpoint,
    path,
    key,
    token,
    max_age,
  };
  observe_worker(ctx) >|= (() => ZMQ.Context.terminate(ctx.zmq_ctx));
};