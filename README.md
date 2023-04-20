### mount a thumb drive
```
mount /dev/sdb1 thumb
```

### create large files
```
cd tmp
dd if=/dev/random of=300mb bs=1M count=300 
ls -lh 300mb 
 ```

 We can notice uniform 

 ### pathological micro bench mark
 If the application accesses data in a reverse order,
 it might have to do a bunch of seeks, which are eliminated since there are no seeks 
 in io_uring. Since all the queue deapth is programmed at once, we might hide some disk 
 latency for the io_uring.

 ### performance test
 We might need to see the impact of disk read and disk write, like author did in the talk.


 ### What is the impact of block sizes?
 * We showed that our copy utilitie's performance is comparable to that of the cp utility.
 * Hence, we can modify the copy the utility to see the impact of the block size.
 * I guess that it should not have an impact because I don't see performance improvement 
 by just increasing the block size.

#### Create a virtual disk
```
sudo mkdir /mnt/virtualdisk
sudo dd if=/dev/zero of=/mnt/virtualdisk/vdisk.img bs=1M count=100
sudo losetup /dev/loop0 /mnt/virtualdisk/vdisk.img
sudo mkfs.ext4 /dev/loop0 
```
#### Mount the virtual disk
```
sudo mount /dev/loop0 /mnt/virtualdisk
sudo dd if=/dev/urandom of=virtualdisk/largefile bs=1G count=4
```

#### Write something to the file and sync
```
echo "Hello World" > /mnt/virtualdisk/largefile
sync
```

#### Unmount the virtual disk to flush the file
```
sudo umount virtualdisk
sudo losetup -d /dev/loop40
```

### Check if a file is in buffer cache
```
sudo fincore /path/to/file
```