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