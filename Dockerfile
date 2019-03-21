FROM alpine:3.9

RUN apk update && apk add alpine-sdk bash ncurses-dev m4 perl gmp-dev zlib-dev libsodium-dev opam zeromq-dev
RUN opam init --disable-sandboxing -ya --compiler 4.07.1
RUN opam install -y depext
RUN opam depext -i -y oml reason ezjsonm lwt_log lwt-zmq bitstring tls ssl irmin-unix

ADD src src
RUN cd src && opam config exec -- dune build --profile release ./main.exe

FROM alpine

RUN adduser -D -u 1000 databox

WORKDIR /home/databox
COPY --from=0 /src/_build/default/main.exe ./logger

RUN apk update && apk upgrade \
&& apk add libsodium gmp zlib libzmq openssl

USER databox

RUN openssl req -x509 -newkey rsa:4096 -keyout /tmp/server.key -out /tmp/server.crt -days 3650 -nodes -subj "/C=UK/ST=foo/L=bar/O=baz/OU= Department/CN=example.com"

EXPOSE 8000


