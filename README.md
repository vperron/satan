# satan

No-bullshit remote management tool for OpenWRT.

* Lightweight
* NAT-traversing
* Remotely DOWNLOAD files, EXEC stuff.
* Uses the awesome [ZMQ3](http://www.zeromq.org/) protocol 
* Future releases: will use ZMQ3 new security layer.

## Architecture

*satan* [UCI](http://wiki.openwrt.org/doc/uci) configuration file lets you define two ZeroMQ endpoints:

* a SUBSCRIBE socket which will enable PUSH notifications from server to be received
* a PUSH socket to send any kind of information back.

Those options can also be set from the command line.

## Command structure

satan allows you to send commands wrapped into ZeroMQ frames.

* Each command is prefixed by the remote device uuid and a message id
* Every answer (sometimes several) to a command is also prefixed by the device id and the message id.

Those commands are represented in the following [ABNF](http://www.ietf.org/rfc/rfc2234.txt) grammar [D stands for device, S for server] :

### Server commands

```
S:satan-pub = uuid msgid command checksum

command =  ( download / exec ) 

download   = 'EXEC'     command
command    = 'DOWNLOAD' binaryblob
```

* `uuid` is the private device uuid to address to
* `msgid` a unique ID to the message for its proper ACK
* `checksum` is a control sum of the message
* `binaryblob` is any arbitrary binary blob, really.

DOWNLOAD will actually copy the `binaryblob` bytes into a `/tmp/<msgid>` file.

### Client answers

```
D:satan-req = uuid msgid answer | uuid zeroedmsgid "UNREADABLE" originalmsg

answer  = ( "MSGACCEPTED" / 
						"MSGPENDING" /
						"MSGCMDOUTPUT" /
						"MSGCOMPLETED" /
						"MSGBADCRC" /
						brokenurl /
						parseerror /
						execerror /
						ucierror /
						undeferror

parseerror = "MSGPARSEERROR"
execerror  = "MSGEXECERROR" ( package / executable / script )
undeferror = "MSGUNDEFERROR" 
```

Note that if a message is _HEAVILY_ unreadable -meaning we did not even succeed
to read up to the message id, we send it back with a zeroed `msgid`.

Note that the device may send:
* `MSGACCEPTED` in a first round, to notify the server that the message had an acceptable format
* `MSGPENDING` is used to signal that the message was the source of a time-consuming operation that may finish later on.
* `MSGCMDOUTPUT` to notify the server of additional output the command may generate
* `MSGCOMPLETED` as soon as the operation is finished; however `COMPLETED` does not make much sense for a firmware upgrade.

## Compile

### As an OpenWRT package:

The associated OpenWRT package makefile can be found under `files/Makefile`.
It takes care of making `satan` run at OpenWRT boot as well.
You can place it directly under a `packages/satan/` folder of your OpenWRT root.

```bash
rm dl/satan-xxxxx.tar.bz2
make package/satan/install V=99
scp bin/MACHINE_ARCH/packages/satan_xxxxx-1_ramips.ipk root@[remoterouter]:.
ssh root@[remoterouter]
```

Once connected,

```bash
opkg install satan_xxxxxx-1_ramips.ipk
```

And you're done.

### On the local machine:

There is almost no use of satan if not on the remote router.
Still, you can still try and compile it for fun or testing purposes, there is a standard autotools-powered compilation process.
satan depends on czmq (and an underlying ZeroMQ v3.x) and uci packages to build.

```bash
./autogen.sh
./configure --with-xxx=[...]
make
```

## Getting Started on OpenWRT

* Install the package.
* The app automatically imports its configuration from `/etc/config/satan` UCI file.
* You can override some parameters; run  `satan -h` to check that out.

## Examples

Run satan onto a different zeromq SUBSCRIBE endpoint, PUSH on some other endpoint, with a custom UUID:

```bash
satan -s tcp://myserver:7889 -p tcp://localhost:1337 -u 9f804aa8172944c683e7213e4d941850
```

## UCI options

* satan.info.thing_uid
The thing uid is also the SUBSCRIBE topic satan listens to.
It should be a 32-byte unique ID.

* satan.subscribe.endpoint
Endpoint on which satan listens to.

* satan.subscribe.hwm
High-water mark to use on SUB socket.

* satan.subscribe.linger
Linger to use on SUB socket.

* satan.answer.endpoint
Endpoint that satan uses to PUSH answer messages.

* satan.answer.hwm
High-water mark to use on PUSH socket.

* satan.answer.linger
Linger to use on PUSH socket.

## License

Free use of this software is granted under the terms of the GNU Lesser General Public License (LGPL). For details see the files COPYING and COPYING.LESSER included with satan.
