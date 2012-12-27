# satan

satan is the daemon responsible of configuration updates and remote upgrade of the boxes.
Needless to say, that particular piece of software has to be particularly stable.
It has life and death rights over [grideye](https://github.com/vperron/grideye) and [snow](https://github.com/vperron/snow) daemons, and can also trigger a full firmware update.

It receives its configuration via an always-connected, dynamic and NAT-traversing zeromq connection to Locarise servers.
Moreover, satan makes use of the [LSD](https://github.com/vperron/lsd) library to make all its unconnected minions in the same network receive the same updates.

## Compile

### As an OpenWRT package:

```bash
rm dl/satan-0.0.1.tar.bz2
make package/satan/install V=99
scp bin/ramips/packages/satan_0.0.1-1_ramips.ipk root@[remoterouter]:.
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

* satan.lsd.group
Group used for all satan daemons in the network to transmit config.
Default value: "xconfig"
