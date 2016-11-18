# MockFS

# About

MockFS solves the issue of working with large production filesystems during migration projects.
In general, you want to be able to test your migration, which moves files around etc., but your not so interested in the actual contents of the files.

MockFS allows you to mimic a production filesystem on your laptop, without taking up much space.

# How does it work?
* On your production environment you create a file index.
This file index contains all the metadata of the file, without storing the contents of it.

* You transfer this index file (sqlite db) to your development machine, and mount is as a filysystem (fuse).

# Install
You'll need to install a few dependencies with your favorite package manager:
e.g. apt-get install cmake libfuse-dev libsqlite3-dev

Then you can build the project:
```sh
cmake .
make
```
And install:
```sh
sudo make install
```
# Usage
* On production, run mfs-index in the folder you want to index. This will create a file 'fileindex' in the folder where you executed it.
* Transfer the 'fileindex' file to your local machine:
  scp production:/path/to/fileindex /local/path/to/fileindex
* Create the directory where you want the mockfs root:
  e.g. mkdir -p /mnt/mockfs
* Mount the filesystem run mfs-mount:
mfs-mount /local/path/to/fileindex /mnt/mockfs
