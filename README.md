# satan

*satan* is a lightweight, NAT-traversing remote management tool for OpenWRT. Following the [KISS](http://en.wikipedia.org/wiki/KISS_principle) principle, it uses a very simple text-based grammar, so writing a remote control tool or collection of scripts should be really straightforward.

Examples of such command tools are given in the `python/` folder.

*satan* has been released after many painful and unsuccessful attempts to manage easily and in a "2.0" fashion remote OpenWRT devices. Either client-side or server-side, there are little to no free, up-to-date, or even just easy-to-implement TR-69/Device:2/CWMP/OMA-DM/SNMP/whatever solutions that were truly satisfying.

The other awesome tools available today ([Puppet](https://puppetlabs.com/), [Chef](http://www.opscode.com/chef/), [SaltStack](http://saltstack.com), ...) are unfortunately not usable on such resource-restricted (4MB flash usually) devices.

*satan* for sure does not have the ambition to tackle as many issues as the previously noted tools do, but has the benefit of an incredible simplicity.

The messaging protocol in *satan* uses the latest awesome [ZMQ3](http://www.zeromq.org/) protocol to reliabily receive PUB notifications from the remote server and PUSH back appropriate answers.

The grammar is very-simple and human-readable.

## Architecture

*satan* [UCI](http://wiki.openwrt.org/doc/uci) configuration file lets you define two ZeroMQ endpoints:

* a SUBSCRIBE socket which will enable PUSH notifications from server to be received
* a PUSH socket to send any kind of information back.

Those options can also be set from the command line.

*satan* is responsible for and able to:

* Execute an arbitrary command, just like a remote shell (but server-initiated)
* Modify an UCI configuration line
* Replace any file in the system
* Update a package
* Update the whole firmware
* Execute a shell script

Each command is prefixed by the remote device uuid and a message id, so that both the server and the device know who it is talking to / receiving answers from / which message is being answered. 

Those commands are represented in the following [ABNF](http://www.ietf.org/rfc/rfc2234.txt) grammar [D stands for device, S for server] :

```
S:satan-pub = uuid msgid command checksum

command =  ( urlfirm / binfirm / 
						urlpak / binpak / 
						urlscript / binscript /
						urlfile / binfile /
						uciline / status )

command   = 'COMMAND'   command

urlfirm   = 'URLFIRM'   httpurl upgradeoptions
urlpak    = 'URLPAK'    httpurl *executable
urlfile   = 'URLFILE'   httpurl destpath
urlscript = 'URLSCRIPT' httpurl

binfirm   = 'BINFIRM'   binary upgradeoptions
binpak    = 'BINPAK'    binary *executable
binfile   = 'BINFILE'   binary destpath
binscript = 'BINSCRIPT' binary

uciline   = 'UCILINE'   optionname'='optionvalue *executable
discover  = 'DISCOVER'
```

* `uuid` is the private device uuid to address to
* `msgid` a unique ID to the message for its proper ACK
* `checksum` is a control sum of the message
* `httpurl` is an URL where the target file, package, firmware can be downloaded from
* `binary` is the full binary script, file, package... transmitted via zeromq
* `upgradeoptions` are some options for the firmware upgrade (undefined yet)
* `executable` is the name of a _service_ to stop and relaunch as in `/etc/init.d/SERVICE start-stop`
* `destpath` is the destination file where to copy the new one
* `optionname` the UCI option name to update
* `optionvalue` the UCI option value to be set.
* `command` is a string (with spaces, line feeds, anything that fits into a ZMQ message frame)

```
D:satan-req = uuid msgid answer | uuid zeroedmsgid "UNREADABLE" originalmsg

answer  = ( "ACCEPTED" / 
						"COMPLETED" /
						"BADCRC" /
						"HELLO" /
						"CMDOUTPUT" /
						brokenurl /
						parseerror /
						execerror /
						ucierror /
						undeferror

brokenurl  = "BROKENURL" httpurl
parseerror = "PARSEERROR"
execerror  = "EXECERROR" ( package / executable / script )
ucierror   = "UCIERROR" optionname
undeferror = "UNDEFERROR" 
```

There above, bote that if a message is _HEAVILY_ unreadable -meaning we did not even succeed
to read up to the message id, we send it back with a zeroed `msgid`.

Note that the device may send:
* `ACCEPTED` in a first round, to notify the server that the message had an acceptable format
* `CMDOUTPUT` to notify the server of additional output the command may generate
* `COMPLETED` as soon as the operation is finished; however `COMPLETED` makes no much sense in two cases:
** `DISCOVER` requests, the completeness is a simple HELLO.
** Successful firmware update requests....

## Compile

### As an OpenWRT package:

The associated OpenWRT package makefile can be found under `files/Makefile`.
It takes care of making `satan` run at OpenWRT boot as well.

```bash
rm dl/satan-0.1.0.tar.bz2
make package/satan/install V=99
scp bin/MACHINE_ARCH/packages/satan_0.1.0-1_ramips.ipk root@[remoterouter]:.
```

### On the local machine:

There is almost no use of satan if not on the remote router.
Still, you can still try and compile it for fun or testing purposes, there is a standard autotools-powered compilation process.

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
