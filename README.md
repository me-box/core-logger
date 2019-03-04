# Databox logger

A web-based interface to collate audit information from Databox components such as [stores](https://github.com/me-box/zestdb). Data is collected using the [Zest protocol](https://arxiv.org/abs/1902.07009) and stored to a time series database.

## Getting started

## starting a store

We will start zestdb with logging enabled to see the interaction.

```bash
docker run -p 5555:5555 -p 5556:5556 -it --name zest --rm jptmoore/zestdb /app/zest/server.exe --secret-key-file example-server-key --enable-logging
```

## starting the logger

We will start the web server on its default port of 8000 without TLS enabled.

```bash
docker run -it --network host -it --name logger --rm jptmoore/zestlogger /home/databox/logger
```

## request audit information

Logging takes place through Zest [observations](https://github.com/me-box/zestdb/tree/master/docs#observation). To setup an observation we need to make a PUT request with some JSON. The length of an observation is determined by the *max-age* field value specified in seconds. A zero value means the observation will not timeout. The PUT request will return immediately so you should monitor the console for issues. The PUT request requires the id of the datasource you want to observe supplied in the path, for example, */observe/foo*.

```bash
docker run --network host --rm byrnedo/alpine-curl -X PUT 127.0.0.1:8000/observe/foo --data '[{"path": "/ts/foo"}, {"key": "vl6wu0A@XP?}Or/&BR#LSxn>A+}L)p44/W[wXL3<"}, {"main_endpoint": "tcp://127.0.0.1:5555"}, {"router_endpoint": "tcp://127.0.0.1:5556"}, {"max_age": 60}, {"token": ""}]'
```

## Interact with store

Once we have setup some observations we can post to the store to generate some log entries. For example, below we post 5 times to the store. 

```bash
docker run --network host -it jptmoore/zestdb /app/zest/client.exe --server-key 'vl6wu0A@XP?}Or/&BR#LSxn>A+}L)p44/W[wXL3<' --path '/ts/foo' --mode post --payload '{"value": 42}' --loop 5
```

## read console log

The log has the format: *timestamp*, *server-name*, *client-name*, *method*, *uri-path*, *response-code*

```bash
Starting observation to foo
1551272344801 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272345810 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272346817 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272347825 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
1551272348830 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65
```

## read from database

We can use the time series API to read the stored logs.

```bash
docker run --network host --rm byrnedo/alpine-curl localhost:8000/ts/foo/latest
```

produces:

```json
[{"timestamp":1551272348812208,"data":{"value":"1551272348830 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65"}}]
```


## combining logs

We can combine logs by comma-separating the the id of each data source when using an API call.

Let's add data to another time series first.

```bash
docker run --network host -it jptmoore/zestdb /app/zest/client.exe --server-key 'vl6wu0A@XP?}Or/&BR#LSxn>A+}L)p44/W[wXL3<' --path '/ts/bar' --mode post --payload '{"value": 42}' --loop 5
```

We can now retrieve data from both time series. Note, this request is equivalent to calling */ts/foo,bar/earliest*.

```bash
docker run --network host --rm byrnedo/alpine-curl localhost:8000/ts/foo,bar/first/1
```

produces:

```json
[{"timestamp":1551272344787363,"data":{"value":"1551272344801 c2480de2b3e0 linuxkit-025000000001 POST /ts/foo 65"}},{"timestamp":1551364092420804,"data":{"value":"1551364092431 b160be0536a4 linuxkit-025000000001 POST /ts/bar 65"}}]
```

## Time series API


#### Read latest entry
    URL: /ts/<id>/latest
    Method: GET
    Parameters: replace <id> with an identifier
    Notes: return the latest entry
    
#### Read last number of entries
    
    URL: /ts/<id>/last/<n>
    Method: GET
    Parameters: replace <id> with an identifier, replace <n> with the number of entries
    Notes: return the number of entries requested
    

#### Read earliest entry
    URL: /ts/<id>/earliest
    Method: GET
    Parameters: replace <id> with an identifier
    Notes: return the first entry
    
#### Read first number of entries
    
    URL: /ts/<id>/first/<n>
    Method: GET
    Parameters: replace <id> with an identifier, replace <n> with the number of entries
    Notes: return the number of entries requested    
    
#### Read all entries since a time (inclusive)
    
    URL: /ts/<id>/since/<from>
    Method: GET
    Parameters: replace <id> with an identifier, replace <from> with epoch milliseconds
    Notes: return entries from time provided
    
#### Read all entries in a time range (inclusive)
    
    URL: /ts/<id>/range/<from>/<to>
    Method: GET
    Parameters: replace <id> with an identifier, replace <from> and <to> with epoch milliseconds
    Notes: return entries in time range provided
    

#### Delete all entries since a time (inclusive)
    
    URL: /ts/<id>/since/<from>
    Method: DELETE
    Parameters: replace <id> with an identifier, replace <from> with epoch milliseconds
    Notes: deletes entries from time provided
    
#### Delete all entries in a time range (inclusive)
    
    URL: /ts/<id>/range/<from>/<to>
    Method: DELETE
    Parameters: replace <id> with an identifier, replace <from> and <to> with epoch milliseconds
    Notes: deletes entries in time range provided
    
#### Length of time series

    URL: /ts/<id>/length
    Method: GET
    Parameters: replace <id> with an identifier
    Notes: return the number of entries in the time series




