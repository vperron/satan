# satan

*satan* is the daemon responsible of configuration updates and remote upgrade of the boxes.
Needless to say, that particular piece of software has to be particularly stable.
It has life and death rights over [grideye](https://github.com/vperron/grideye) and [snow](https://github.com/vperron/snow) daemons, and can also trigger a full firmware update.

It receives its configuration via an always-connected, dynamic and NAT-traversing zeromq connection to Locarise servers.

## Architecture

*satan* is responsible for and able to:

* Modify an UCI configuration line
* Replace any file in the system
* Update a package
* Update the whole firmware
* Execute a shell script
* Report device status

For this, *satan* receives commands from a _zeromq_ SUBSCRIBE socket, on its device-specific `device.info.uuid` channel.

Each command has to be acknowledged or answered by *satan* on its REQ upstream socket.

Those commands are represented in the following [ABNF](http://www.ietf.org/rfc/rfc2234.txt) grammar [D stands for device, S for server] :

```
S:satan-pub = uuid msgid command checksum

command =  ( urlfirm / binfirm / 
						urlpak / binpak / 
						urlscript / binscript /
						urlfile / binfile /
						uciline / status )

urlfirm   = 'URLFIRM'   httpurl upgradeoptions
urlpak    = 'URLPAK'    httpurl *executable
urlfile   = 'URLFILE'   httpurl destpath
urlscript = 'URLSCRIPT' httpurl

binfirm   = 'BINFIRM'   binary upgradeoptions
binpak    = 'BINPAK'    binary *executable
binfile   = 'BINFILE'   binary destpath
binscript = 'BINSCRIPT' binary

uciline   = 'UCILINE'   optionname'='optionvalue *executable
status    = 'STATUS'
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



```
D:satan-req = uuid msgid answer | uuid zeromsgid "UNREADABLE" originalmsg

answer  = ( "ACCEPTED" / 
						"COMPLETED" /
						status /
						"BADCRC" /
						brokenurl /
						parseerror /
						execerror /
						ucierror /
						undeferror

status     = "STATUS" fullstatus

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
* `COMPLETED` as soon as the operation is finished; `COMPLETED` makes no sense in two cases:
** `STATUS` request, the completeness is the answer itself
** Firmware updates, obviously keeping the message id before and after is not that necessary.

In turn, the device will wait for a last ACK from the server.

```
S:satan-rep = uuid msgid "OK"
```

### Device status

`fullstatus` stated above SHALL contain, and is not limited to:

* the memory state of the device ( `cat /proc/meminfo` )
* the current load snapshot of the device ( `top -n 2` )
* the current active processes ( `ps` ) 
* the disk space occupancy ( `df` )

## Compile

### As an OpenWRT package:

```bash
rm dl/satan-0.2.0.tar.bz2
make package/satan/install V=99
scp bin/ramips/packages/satan_0.2.0-1_ramips.ipk root@[remoterouter]:.
```

### On the local machine:

There is kind of no use of satan if not on the remote router.
You cans still try and compile it for fun, though; it's a fully standard autotools-powered compilation process.
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

Run satan onto a different zeromq endpoint:

```bash
satan -e tcp://iso3103.net:666
```

## UCI options

* satan.subscribe.endpoint
Endpoint where the updated are listened to.
Default value: tcp://iso3103.net:10079

* satan.subscribe.hwm
High-water mark to use on SUB socket.
Default value: 2

* satan.subscribe.linger
Linger to use on SUB socket.
Default value: 0
