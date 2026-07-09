# LED Matrix Panel

Software to drive vinatge LED panels

## initial investigation

[Arduino Demo](ArduinoDemo.md)

## rp2040 pico-w full panel driver

The bus station led matrix display now has WiFi, initially it acts as a station which you can connect to to enter your local router details to make a permanent connection. 

![wifi setup](images/wifi_setup.png)

When you have made a connection to the AP using the SSID shown, then opened the web page at the address given a menu of options is presented:

![web portal](images/web_portal.png)

In the menu you can scan for local wifi networks and select new text to appear on the display, special text substitutions exist ^g to ring the external bell, and $TIME$ to show the time from a time server. Here the display updated with the text from the configuration:

![demo](images/demo.png)

Any connected wifi station holds its own panel variables, including text and brightness, the actual station can be chosen in the webpage but a scan of available station is used to choose the station to connect to at boot time, the default is the initial configuration access point. The system can be configured to simply only display the initial text if no wifi is available, all the adjustable settings and the web page are held in files on the micro itself

It was suggested to me that the display could show a QR code to help to get it connected but because of the limited resolution and mainly because of the gaps between the lines I couldn’t get this to work. Here is a photo of my QR code attempt, it uses custom characters to form the bitmap in sections.

[qrcode](images/qrcode_test.png)

There is a possibility of making a linear barcode to display on the LEDs, I’ll bee trying that but in the meantime the display can show the information needed to log onto its wifi:

I shall probably need to extend the micro’s wifi aerial to the front panel perspex since it sits behind the metal chassis which will inside the metal case in the assembled unit.

## Interconnnections

|Connector|Chip   |Name   |Pin#|GPIO|Colour      |
|---------|-------|-------|----|----|------------|
|         |LVC245 |       |    |    |            |
|C        |7-13   |Data 4 |   9|G6  |Yellow4     |
|B        |6-14   |Data 3 |   7|G5  |Yellow3     |
|A        |5-15   |Data 2 |   6|G4  |Yellow2     |
|1        |3-17   |Clock  |   4|G2  |Brown       |
|2        |9-11   |Latch  |  19|G14 |Red         |
|3        |2-18   |Enable |  20|G15 |Orange      |
|4        |4-16   |Data 1 |   5|G3  |Yellow      |
|5        |       |+ve    |    |5v  |Green       |
|6        |       |-ve    |    |Gnd |Blue        |
|7        |       |-ve    |    |Gnd |Mauve       |
|         |TD6273n|       |    |    |            |
|8        |7      |Row 7  |  17|G13 |brown/brown |
|9        |1      |Row 1  |  10|G7  |brown/red   |
|10       |2      |Row 2  |  11|G8  |brown/orange|
|11       |3      |Row 3  |  12|G9  |brown/yellow|
|12       |4      |Row 4  |  14|G10 |brown/green |
|13       |5      |Row 5  |  15|G11 |brown/blue  |
|14       |6      |Row 6  |  16|G12 |brown/mauve |
|         |       |       |    |    |            |
|n/a      |8      |LED    | n/a|    |            |
|n/a      |button |Config |  16|    |            |
|n/a      |sounder|Bell   |  17|    |            |
|n/a      |VFD    |V-rst  |  18|    |            |
|         |VFD    |V-tx   |  19|    |            |
|         |VFD    |V-bsy  |  20|    |            |
|         |LDR    |LDR    |  26|    |            |
|         |LDR    |3v3 PU |    |    |            |



