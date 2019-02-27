# Databox logger

A web-based interface to collate audit information from Databox components such as [stores](https://github.com/me-box/zestdb). Data is collected using the [Zest protocol](https://arxiv.org/abs/1902.07009) and stored to a time series database.

## Getting started

## starting a store

```bash
docker run -p 5555:5555 -p 5556:5556 -it --name zest --rm jptmoore/zestdb /app/zest/server.exe --secret-key-file example-server-key --enable-logging
```

## starting the logger

```bash
todo: dockerize
```

## request audit information

```bash
curl -X PUT localhost:8000/observe/foo --data '[{"path": "/ts/foo"}, {"key": "vl6wu0A@XP?}Or/&BR#LSxn>A+}L)p44/W[wXL3<"}, {"main_endpoint": "tcp://127.0.0.1:5555"}, {"router_endpoint": "tcp://127.0.0.1:5556"}, {"max_age": 60}, {"token": ""}]'
```

## POST to store

```bash
docker run --network host -it jptmoore/zestdb /app/zest/client.exe --server-key 'vl6wu0A@XP?}Or/&BR#LSxn>A+}L)p44/W[wXL3<' --path '/ts/foo' --mode post --payload '{"value": 42}' --loop 5
```

## read console log

```bash
Starting observation to foo
1551272344801 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272345810 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272346817 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272347825 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272348830 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
```

## read from database

```bash
curl localhost:8000/ts/foo/latest
```

produces:

```json
[{"timestamp":1551272348812208,"data":{"value":"1551272348830 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65"}}]
```






