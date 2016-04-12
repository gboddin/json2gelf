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

###Help

```
$ ./json2gelf --help

Usage: json2gelf [OPTION...]
json2gelf -- Forward line separated JSON to a UDP GELF server

  -f, --file=FILE            Input file (default: /dev/stdin)
  -h, --host=HOST            GELF remote server (default: 127.0.0.1)
  -n, --no-validate-json     Skip JSON packet checks (default: no)
  -p, --port=PORT            GELF remote port (default: 12345)
  -v, --verbose              Increase verbosity
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <gregory@siwhine.net>.
```


###Helper scripts


Some scripts are already in the [scripts folder](scripts), but you just need to pipe valid json packets to it.

```
./scripts/varnish2json|./json2gelf
```

##Warnings

- Logstash currently has a bug making the JSON parser crash if JSON is badly formated, it also crashes the gelf listener in the process ... : [Issue link](https://github.com/logstash-plugins/logstash-input-gelf/pull/27)
