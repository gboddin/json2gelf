#json2gelf

##Description

Gets JSON lines from stdin and forward it as GELF packet.

##Compilation

First edit json2felf.c to your needs by tweaking SERVER, PORT and UDP_CHUNK_SIZE, then:

```
make clean
make
```

##Usage

Some scripts are already in the [scripts folder](scripts), but you just need to pipe valid json packets to it.

```
./scripts/varnish2json|./json2gelf
```

##Warnings

- Logstash currently has a bug making the JSON parser crash if JSON is badly formated, it also crashes the gelf listener in the process ... : [Issue link](https://github.com/logstash-plugins/logstash-input-gelf/pull/27)
