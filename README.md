[![Build Status](https://travis-ci.org/rpodgorny/unionfs-fuse.svg?branch=master)](https://travis-ci.org/rpodgorny/unionfs-fuse)
[![Liberapay](https://liberapay.com/assets/widgets/donate.svg)](https://liberapay.com/rpodgorny/donate)

unionfs-fuse
============

This is my effort to create a unionfs filesystem implementation which is way more
flexible than the current in-kernel unionfs solution.

I'm open to patches, suggestions, whatever...

The preferred way is the mailing list at unionfs-fuse@googlegroups.com
or see http://groups.google.com/group/unionfs-fuse.

Why choose this stuff
---------------------

* The filesystem has to be mounted after the roots are mounted when using the standard module. With unionfs-fuse, you can mount the roots later and their contents will appear seamlesly
* You get caching (provided by the underlying FUSE page cache) which speeds things up a lot for free
* Advanced features like copy-on-write and more

Why NOT choose it
-----------------

* Compared to kernel-space solution we need lots of useless context switches which makes kernel-only solution clear speed-winner (well, actually I've made some tests and the hard-drives seem to be the bottleneck so the speed is fine, too)

How to build
------------

You can either use plain make or cmake (pick one).

1. plain make

Just issue `make` - this compiles the code with some static settings (xattrs enabled, ...) tuned for my linux system.

2. cmake

```
mkdir build; cd build
cmake ..
make
```

This should allow for compilation on wider variety of systems (linux, macos, ...) and allows to enable/disable some features (xattrs, ...).
