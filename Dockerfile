FROM ocaml/opam2

RUN sudo apt-get install -y time m4
RUN opam depext -i oml reason ezjsonm lwt_log lwt-zmq bitstring tls ssl irmin-unix
RUN opam install --unlock-base ppx_bitstring

ADD src src
RUN sudo chown -R opam:nogroup src
RUN cd src && opam config exec -- dune build --profile release ./main.exe

FROM debian

RUN useradd -ms /bin/bash databox

WORKDIR /home/databox
COPY --from=0 /src/_build/default/main.exe ./logger

RUN apt-get update && apt-get install -y libgmp10 libssl1.1 zlib1g openssl libzmq3

USER databox

RUN openssl req -x509 -newkey rsa:4096 -keyout /tmp/server.key -out /tmp/server.crt -days 3650 -nodes -subj "/C=UK/ST=foo/L=bar/O=baz/OU= Department/CN=example.com"

EXPOSE 8000


