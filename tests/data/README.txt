# Machine code -> assembly

### make .bin first
```shell
avr-objcopy -I ihex -O binary blink.hex blink.bin
```

### decompile
```shell
avr-objdump -D -b binary -m avr blink.bin > blink.txt
```