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
* You get caching which speeds things up a lot for free
* You get nice stats (optional)
* Advanced features like copy-on-write and more

Why NOT choose it
-----------------

* Compared to kernel-space solution we need lots of useless context switches which makes kernel-only solution clear speed-winner (well, actually I've made some tests and the hard-drives seem to be the bottleneck so the speed is fine, too)
