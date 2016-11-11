# MockFS

# About

MockFS solves the issue of working with large production filesystems during migration projects.
In general, you want to be able to test your migration, which moves files around etc., but your not so interested in the actual contents of the files.

MockFS allows you to mimick a production filesystem on your laptop, without taking up much space.

# How does it work?
* On your production environment you create a file index.
This file index contains all the metadata of the file, without storing the contents of it.

* You transfer this index file (sqlite db) to your development machine, and mount is as a filysystem (fuse).

# Install
You'll need install a few depenencies with your favorite package manager:
e.g. apt-get install cmake libfuse-dev libsqlite3-dev

Then you can build the project:
```sh
cmake .
make
```

# Usage
* On production, run mfs-index in the folder you want to index. This will create a file 'fileindex'.
* Transfer the 'fileindex' file to your local machine.
* Create the directory where you want the mockfs root:
  e.g. mkdir -p /mnt/mockfs
* From the directory where 'fileindex' is stored, run mfs-mount:
mfs-mount /mnt/mockfs -s -f (this runs MockFS in a single thread, and keeps it in the foreground)

Note: Currently, only single thread mode is supported due to a bug in MockFS. Pull requests are welcome ;-)
