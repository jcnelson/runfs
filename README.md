runfs
=====

Runfs is a self-cleaning filesystem that allows a process to store temporary state in the filesystem over the course of its lifetime.  Once it dies, the filesystem removes its data.

Dependencies
------------
* OpenSSL (for SHA256)
* [fskit](https://github.com/jcnelson/fskit)

Building
---------

To build:
    $ make

Installing
----------

To install:
    $ sudo make install

By default, it will be installed to /usr/local/bin

Running
-------

To run:
    $ ./runfs /path/to/mountpoint

It takes FUSE arguments like -f for "foreground", etc.  See 'fuse(8).'
