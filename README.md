# Before running
This demonstration will require super user, so make sure you read and understand the code before trying it.

# Running
## In output terminal
```console
$ make demo
mkdir -p build;
gcc watch-fs.c -o build/watch-fs
mkdir -p demo/mount
rm -f demo/fs.img
truncate -s64M demo/fs.img
mkfs.ext4 demo/fs.img
mke2fs 1.47.3 (8-Jul-2025)
Discarding device blocks: done                            
Creating filesystem with 65536 1k blocks and 16384 inodes
Filesystem UUID: dec7c5f0-f6fe-4078-bf3d-a1960f8c5474
Superblock backups stored on blocks: 
        8193, 24577, 40961, 57345

Allocating group tables: done                            
Writing inode tables: done                            
Creating journal (4096 blocks): done
Writing superblocks and filesystem accounting information: done

sudo mount demo/fs.img demo/mount
[sudo] password for devilholk: 
sudo chown `id -nu`:`id -ng` demo/mount/
sudo ./build/watch-fs ./demo/mount; \
sudo umount demo/mount
Listening for events.
Press enter key to terminate.
```

## In experiment terminal
- Navigate to `demo/mount`
- Create, move, delete and write files and directories here and observe output in the output terminal.

### Example
#### Experiment terminal
```console
$ mkdir mahdir
$ mv mahdir/ my_dir
$ touch my_dir/silly-kitteh
```
#### Output terminal
```
Event mask: FAN_ONDIR | FAN_CREATE
  Name: /WORKDIR/demo/mount/mahdir
Event mask: FAN_ONDIR | FAN_RENAME
  Old name: /WORKDIR/demo/mount/mahdir
  New name: /WORKDIR/demo/mount/my_dir
Event mask: FAN_CREATE
  Name: /WORKDIR/demo/mount/my_dir/silly-kitteh
Event mask: FAN_ATTRIB | FAN_CLOSE_WRITE
  Name: /WORKDIR/demo/mount/my_dir/silly-kitteh
```



