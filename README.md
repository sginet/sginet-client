# SGINet client

Copyright (c) 2024 flexion - licensed under the MIT License.

## What is this?

SGINet is a command line client intended for Silicon Graphics MIPS computers running IRIX.

The `sginet` client allows you to:

- Register your SGI machine and share `hinv` data on [sginet.flexion.io](https://sginet.flexion.io)
- Report uptime and system load on [sginet.flexion.io](https://sginet.flexion.io) (optional)
- View the online status and hinv of other users SGI machines
- Take desktop screenshots (uploaded to [sginet.flexion.io](https://sginet.flexion.io) as PNG)
- Search software packages (currently: SGUG-RSE, SGI Freeware CDs and nekoware)
- Browse new sgi.sh forum posts
- Post in the public shoutbox
- View memory usage and CPU load of your SGI machine
- System Advisor: checks your IRIX installation for unnecessary daemons and provides information on configuration improvements.

`sginet` can also be launched as a daemon which reports the system's online status every 12 minutes to the (public) cloud backend.

## Installation

Tardist releases can be downloaded from [sginet.flexion.io](https://sginet.flexion.io/#/download)

Use the IRIX Software Manager (swmgr) to install the `tardist` release package, or `inst -f sginet-CL6.tardist` on the console. 

Emulators: SGINet is intended for real SGI/MIPS hardware. Please do not register emulators. The cloud API silently rejects requests from emulators. 

Integrated online updater: sginet, xsginet and xsginet-globe come with an integrated updater. As root user launch with the -u option to update to the latest published version.

## Starting the SGINet client

The tardist package will install the `sginet` executable in `/usr/bin` which should already be in the search path of a default IRIX installation.

The X11/Motif client `xsginet` is installed in `/usr/bin` too. Register your machine with `sginet` before you launch `xsginet`.

To launch the `sginet` client with its ncurses UI in a terminal:

```sh
sginet
```

On first lanch you will be asked to register your Silicon Graphics computer.

## Terminal settings

If `sginet` refuses to launch and complains about the terminal, set your `$TERM` variable to `iris-ansi` to fall back to the most basic UI.
Best viewed with aterm or mrxvt.

## Cloud connection

This app connects to a REST API in the cloud using TLS 1.2.

## Data privacy

The cloud REST API (AWS) keeps your IP and request time (without payload) for up to three days in the logs for invocation/error metrics. No data will be used for marketing purposes or sold to a third party.

Other users can see the online status of your machine(s) and view some `hinv` data, just as you can see other peoples systems online status and `hinv`.

Screenshots you take using `sginet` are stored in the cloud for up to two years, unless you delete them. Other users can view your screenshots on the sginet website too.

## License

Copyright (c) 2024 flexion

SGINet is licensed under the MIT License, see the [LICENSE](LICENSE) file for details.

Bundled third-party code keeps its own license:

- BearSSL — MIT
- json-c — MIT
- ncurses — X11/MIT-style license

