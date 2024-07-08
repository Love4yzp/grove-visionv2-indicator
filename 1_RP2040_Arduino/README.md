if you trying to use the uart as the harware serial port, see this issue to modify the `pins_arduino.h`

https://github.com/Seeed-Solution/SenseCAP_Indicator_ESP32/issues/49

add the following code in `pins_arduino.h` of `variants\seeed_indicator_rp2040`, and comment other conflict code:

```cpp
#define PIN_SERIAL2_TX (20u)
#define PIN_SERIAL2_RX (21u)
```

or make a copy of the previous `pins_arduino.h` and rename it to `pins_arduino.h.bak`, replace it with the `pins_arduino.h` in this folder.