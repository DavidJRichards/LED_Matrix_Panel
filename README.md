# LED_Matrix_Panel
Software to drive vinatge LED panels

Arduino sketch for Pro Mini board, uses TD6783AP high-side driver to drive LED anode banks. LED bit patterns are sent out using SPI device in micro feeding four chains of cascaded shift registers type 75HC595. The seven lines forming a single row of text aer sent sequentially, then a pause of 1mS to display the image formed. The four text lines are linterleaved so that all four first character lines are sent followed by all four second character lines and so on, this minimises the ammount of multiplexing needed, apart from time taken for additional processing each LEDS can be on for a maximum  of 1/7 of the total time. 

[Demo sketch](LED_Matrix_4Line_Demo/LED_Matrix_4Line_Demo.ino)

[Video](https://youtube.com/shorts/09ZpsXYtBmo?si=rDpB5rVtnLxmoRZ1)