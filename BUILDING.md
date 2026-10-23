
# SGINet client

Copyright (c) 2024 flexion - licensed under the MIT License, see [LICENSE](LICENSE).

## Building

The sginet client builds with native IRIX MIPSPro compiler.
All libraries are linked statically; the binary only needs IRIX libc, libpthread and libm. Do not mix with SGUG-RSE libraries.

Bundled in `third_party/`:

- BearSSL 0.6 (TLS 1.2)
- json-c 0.13.1
- ncurses 6.5 (fetched by `third_party/build-ncurses.sh`, built with form/menu/panel/tinfo)

### IRIX 6.x / MIPSpro

Make sure you have the MIPSpro (7.4.x) compiler installed. 

```sh
sh third_party/build-ncurses.sh
```

And then build the sginet client with smake:

```sh
smake
```

### IRIX 5.x / IDO cc

IRIX 5 needs the compiler from the IDO (C89).

On old systems with small partitions, make sure you have enough space in the partition of the temp directory. To change the temp directory:

```sh
setenv TMPDIR /usr/tmp
```

If the make-tardist-IRIX5.sh script runs out of space, also change the WORK env. variable to point to a directory in a parition with enough space. 

Start building:

```sh
sh third_party/build-ncurses.sh
```

And then build the sginet client with smake:

```sh
smake irix5
```

### xsginet (X11/Motif client)

`xsginet` in `x11-client/` uses the same cloud API code, BearSSL and json-c as `sginet`, and links only against the IRIX Motif 1.2, Xt and X11 libraries.

```sh
make xsginet           # IRIX 6.x / MIPSpro
smake xsginet-irix5    # IRIX 5.x / IDO cc
```

The tardist scripts in `dist/` add `xsginet` as the subsystem `sginet_client.sw.x11` if an `xsginet` binary is found in the top directory.

On IRIX 6.5, `xsginet` and `xsginet-globe` are linked with `-no_library_replacement` (`X11_LDFLAGS`), so rld ignores `LD_LIBRARY*_PATH` to prevent using incompatible `/usr/sgug/lib32` libraries.

### xsginet-globe (OpenGL globe)

`xsginet-globe` in `x11-globe/` uses the same cloud API code as `xsginet`.

```sh
make xsginet-globe           # IRIX 6.x / MIPSpro
smake xsginet-globe-irix5    # IRIX 5.x / IDO cc
```


## TLS certificates

The root certificates used to talk to the sginet cloud (Amazon Root CA 1-4, Starfield Services Root G2) are compiled into the binary
(`ta_builtin.c`, generated from `certs/amazon-roots.pem` with `brssl ta`), so no CA bundle is needed.
`--ca-cert-bundle PATH` adds the certificates of a PEM bundle to the built-in ones.

TLS needs a correct system clock. Certificate validation fails if the date is wrong. If your Dallas clock is dead, replace it or add a `ntpdate` update command in init.d between network and sginet-daemon to adjust the clock on each startup.

If `/dev/urandom` is not available, a fallback entropy source is used (timing jitter, system state and a seed file `~/.sginet_rnd`).


## Debug network activity

To start in debug mode and print REST API payload to a log file:

```sh
(sginet -d > /dev/tty) >& /tmp/sginet-debug.log
```

Or start sh first, due to the 2> issue in csh/tcsh

```sh
sh
sginet -d 2> /tmp/sginet-debug.log
```
