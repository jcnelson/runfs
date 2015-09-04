runfs
=====

Runfs is a self-cleaning in-RAM filesystem where data shares fate with the process that creates it.  Once the process exits, the files it created in runfs are automatically unlinked, as well as its directories and all of their descendents.

Example Use-Cases
-----------------
* Store daemon PID files while guaranteeing that they always disappear when the daemon stops, *even if it crashes*.
* Give cryptographic tools the use of the filesystem while guaranteeing that sensitive data becomes inaccessible once the tool exits.
* Give your favorite build system a place to store its subprocess' temporary files while ensuring that a failed or interrupted build always starts clean.

Dependencies
------------
* [fskit](https://github.com/jcnelson/fskit)
* [libpstat](https://github.com/jcnelson/libpstat)

Building
---------

To build:

        $ make

Installing
----------

To install:

        $ sudo make install

By default, runfs will be installed to `/usr/local/bin`.  You may set `PREFIX` to control the installation directory, and `DESTDIR` to set an alternate installation root.

Running
-------

To run:

        $ ./runfs /path/to/mountpoint

It takes FUSE arguments like -f for "foreground", etc.  See `fuse(8).`
