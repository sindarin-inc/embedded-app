```
make all
./driverdumper-old > old.txt && ./driverdumper-new > new.txt
diff old.txt new.txt
```

Or using delta

```
delta old.txt new.txt
```
