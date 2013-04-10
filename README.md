
satan
=====

Barebone remote management tool for OpenWRT.

* Extremely lightweight: a single deamon of a few KBs.
* NAT-traversing
* Server-initiated PUSH/PULL files, EXEC stuff onto remote device.
* Uses the awesome [ZMQ3](http://www.zeromq.org/) protocol
* Security is coming: will use ZMQ3 new security layer.


Architecture
------------

*satan* receives its commands from a "command server" located at a configurable address.
A zeromq SUBSCRIBE endpoint will be used to receive the commands, and a PUSH zeromq channel is used to send back answers to the server.
The minion itself on the device is identified by an unique UID, that the server uses to address it.

Those options are configurable either on the command line or using an [UCI](http://wiki.openwrt.org/doc/uci) configuration file on OpenWRT.

### Command structure

satan allows you to send commands wrapped into ZeroMQ frames.

* Each command is prefixed by the remote device uuid and a message id
* Every answer (sometimes several) to a command is also prefixed by the device id and the message id.

Those commands are represented in the following [ABNF](http://www.ietf.org/rfc/rfc2234.txt) grammar [D stands for device, S for server] :

#### Server commands

```
S:satan-pub = uuid msgid command checksum

command =  ( push / pull / exec / tasks / kill )

exec   = 'EXEC' <command>
push   = 'PUSH' <binaryblob> [filename]
pull   = 'PULL' <filename>
tasks  = 'TASKS'
kill   = 'KILL' <task_id>

```

###### Parameters

* `uuid` is the private device unique ID to address. Minimum 4 chars.
* `msgid` a unique ID to the message and all of its answers. Minimum 4 chars.
* `checksum` is a control sum for the message arguments, calculated using Paul Hsieh's [superfasthash](http://www.azillionmonkeys.com/qed/hash.html)
* `binaryblob` is any arbitrary binary blob: script, firmware image, package...

###### Command use

* EXEC allows you to run any arbitrary command on the remote device and watch its output from the server.
satan internally keeps track of every task alive; the MSGPENDING message is associated with a `task\_id` that you can us in the KILL command to terminate the task; the MSGCOMPLETED message is issued when the task ends.
* The PUSH command allows you to push any blob of data onto the device. It will be saved into the `/tmp/<msgid>` file unless you soecify the optional `filename` argument.
* Use TASKS command to list the current active tasks on the remote. Every task is associated with its original message ID and complete command, to easily identify it.
* The KILL command enables you to easily kill a task that you find disturbing and remove it from satan's internal task list.
* The PULL command does the opposite; it enables you to retrieve a file from the remote as designated by the `filename` parameter.


#### Client answers

```
D:satan-req = uuid msgid answer | <uuid> <emptymsgid>  'UNREADABLE' <originalmsg>

answer  = ( 'MSGACCEPTED' /
						'MSGCOMPLETED' /
            'MSGEXECERROR' /
            'MSGUNDEFERROR' /
						'MSGBADCRC' <originalmsg> /
            'MSGPARSEERROR' <originalmsg> /
            msgtask /
            cmdoutput /

msgtask    = 'MSGTASK'
cmdoutput  = 'MSGCMDOUTPUT' <cmdoutput>
```

Note that if a message is _HEAVILY_ unreadable -meaning we did not even succeed
to read up to the message id, we send it back with a zeroed `msgid`.

Note that the device may send:
* `MSGACCEPTED` in a first round, to notify the server that the message had an acceptable format
* `MSGTASK` is issued when a task has been created to notify the server of that task's ID.
* `MSGCMDOUTPUT` to notify the server of additional output the command may generate
* `MSGCOMPLETED` as soon as the operation is finished; however `COMPLETED` does not make much sense for a firmware upgrade.


Compile
-------

### Dependencies

satan depends on czmq (and an underlying ZeroMQ v3.x). The OpenWRT build also makes use of libuci to read its unified configuration file.
You can use the collection of [zeromq OpenWRT Makefiles](https://github.com/vperron/openwrt-zmq-packages) to
build zeromq v3 and czmq on your image.

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

```bash
./autogen.sh
./configure --with-xxx=[...]
make
```


Getting Started on OpenWRT
--------------------------

* Install the package.
* The app automatically imports its configuration from `/etc/config/satan` UCI file.
* You can override some parameters; run  `satan -h` to check that out.


Examples
--------

### Command line parameters

Run satan onto a different zeromq SUBSCRIBE endpoint, PUSH on some other endpoint, with a custom UUID:

```bash
satan -s tcp://myserver:7889 -p tcp://localhost:1337 -u my_minion_uid
```

### Update the firmware

OpenWRT boxes typically are wuite limited on the amount of RAM available.
ZeroMQ is a lightweight messenging protocol, and your attemps to transmit a whole firmware embedded into a PUSH command message from the server will almost inevitably fail.
For this reason, it is probably better to make your device donload the firmware and then use `mtd write` to update it. `sysupgrade` tool will fail since its first job is to kill all the deamons (that includes its satan parent if you call it from remote)
Here's a way to go - using the `control` python tool from this repository:

```
./control my_uid EXEC 'cd /tmp/ && wget http://myserver/openwrt-ARCH-DEVICE-xxx-squashfs-sysupgrade.bin'
./control my_uid EXEC 'mtd -r write /tmp/openwrt-ARCH-DEVICE-xxx-squashfs-sysupgrade.bin firmware'
```


UCI options
-----------

* satan.info.uid

The uid is also the SUBSCRIBE topic satan listens to.

* satan.info.subscribe

Endpoint on which satan listens to.

* satan.info.answer

Endpoint that satan uses to PUSH answer messages.

Changelog
---------

### 0.2.2

**Date**: 10th April 2013

* Complete rewrite
* EXEC and PUSH commands available 
* Python tests and examples
* Dependency on libuci made optional
* --enable-debug flag at configure time

### 0.1.0 (28 March 2013)

* Parsing of all the commands is working
* UCILINE and COMMAND commands are actually supported
* Main architecture and tests are present


License
-------

Free use of this software is granted under the terms of the GNU Lesser General Public License (LGPL). For details see the files COPYING and COPYING.LESSER included with satan.
