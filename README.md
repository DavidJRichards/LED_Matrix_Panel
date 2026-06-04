# LED Matrix Panel

![LED_Panel](images/LED_Panel.png)
Software to drive vinatge LED panels

Arduino sketch for Pro Mini board, uses TD6783AP high-side driver to drive LED anode banks. LED bit patterns are sent out using SPI device in micro feeding four chains of cascaded shift registers type 75HC595. The seven lines forming a single row of text aer sent sequentially, then a pause of 1mS to display the image formed. The four text lines are linterleaved so that all four first character lines are sent followed by all four second character lines and so on, this minimises the ammount of multiplexing needed, apart from time taken for additional processing each LEDS can be on for a maximum  of 1/7 of the total time. 

[Demo sketch](LED_Matrix_4Line_Demo/LED_Matrix_4Line_Demo.ino)

[Video](https://youtube.com/shorts/09ZpsXYtBmo?si=rDpB5rVtnLxmoRZ1)

The software is designed to drive four panels in cascade giving four lines of 240x7 LEDS. only a single panel has been tested so far. 

|LED Connector 1/2/3/4 |Name|Arduino Pin|
|-------------|----|-----------|
|1|Clock|13|
|2|Latch|A2/A3/A4/A5|
|3|Enable|12|
|4|Data|11|
|5|+ve|5v|
|6|-ve|Gnd|
|7|-ve|nc|
|8|Row 7|D2|
|9|Row 1|D3|
|10|Row 2|D4|
|11|Row 3|D5|
|12|Row 4|D6|
|13|Row 5|D7|
|14|Row 6|D8|

The signals to all four board connectors are paralled withthe exception to the text line latch signals, these are fed singly from the arduino A2/A3/A4/A5 lines. These connections or similar should be done by the board backplane or whatever is in the origianl housing.

![Boards](images/Boards.png)
