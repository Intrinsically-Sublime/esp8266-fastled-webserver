FastLED + ESP8266 Web Server
=========

### Custom version of Jason Coons FastLED webserver for Novel Mutations Costume Controllers (LiPo battery powered ESP8266 based boards with MSGEQ7 and Microphone)
It will work with any ESP8266 based controller.

#### Some of the changes from original version by Jason Coon.
1) All patterns work with XY matrices including irregular matrices.
2) Fire with custom palettes in their own drop down based on the colors emitted when certain elements are burnt.
3) Rotating color palettes for fire.
4) Removed infrared control.
5) Added WiFi transmission power setting.
6) Remove twinkle functions.
7) Made twinkleFOX use its own palette dropdown to clean up pattern drop down and added a couple of palettes to twinkleFOX from twinkle.
8) Most patterns are now affected by the speed setting.
9) All patterns list which settings affect them (in the drop down next to their name).
10) Remove need for Glyphs, Icons and Fonts by using HTML entities in their place. It now fits on an ESP-01 properly.
11) Removed unused functions to cleanup code.
12) Made all patterns capable of addressing more than 256 LEDs.
13) Rotate palettes after each time through all patterns in autoplay mode.
14) Added Cylindrical option to wrap certain patterns around at the ends (Fireworks, Storm).
15) Added blending to fire to smooth the animation.
16) Physical button control. Up to 5 buttons and 15 function calls (Pattern+/-, Palette/Color+/-, Speed+/-, Brightness+/-, Power, Autoplay on/off, ~~WiFI on/off~~, ~~Audio on/off~~, ~~Scrolling Text on/off~~, ~~Unassigned~~, ~~Unassigned~~). The first 2 buttons are enough to control most features.
17) Intensity slider to control rain intensity and twinkle density.

#### Custom irregular XY map generator based on Garrett Maces
https://intrinsically-sublime.github.io/FastLED-XY-Map-Generator/

## Patterns
##### Fire -- Simulated fire with custom palettes
* Settings
  * Fire Palettes -- See Palettes section below
  * Speed -- Adjusts the speed the heat travels upwards
  * Cooling -- Adjusts the rate at which the heat cells cools
  * Sparking --  Adjusts the frequency of new embers
---
##### Fireworks -- Random fireworks on 2D matrix
* Settings
  * Speed -- Adjusts the velocity of the shells and the gravity pulling down the sparks
---
##### Rain -- Colored rain falling down from the top and splashing at the bottom
* Settings
  * Speed -- Adjusts the rate of rainfall
  * Color Picker -- Sets the color of the rain
  * Intensity -- Adjusts the intensity of the rain
---
##### Storm -- Colored rain falling down from clouds at the top and splashing at the bottom with peridodical lightning bolts
* Settings
  * Speed -- Adjusts the rate of rainfall
  * Color Picker -- Sets the color of the rain
  * Intensity -- Adjusts the intensity of the rain
---
##### The Matrix -- Green dots raining down from the top
* Settings
  * Speed -- Adjusts the rate the dots move from top to bottom
  * Intensity -- Adjusts the intensity of the dots
---
##### Pride -- Ever changing 2D interference patterns
* Settings
  * Speed -- Adjusts the rate of movement of the interference patterns
---
##### Rainbow -- Flowing rainbow over the height of the matrix
---
##### Rainbow w/ Glitter -- Flowing rainbow over the height of the matrix with sparkling glitter added
---
##### Solid Rainbow -- Slowly changing solid colors from a palette
* Settings
  * General Palettes -- See Palettes section below
---
##### Color Waves -- Flowing waves of color over the height of the matrix
---
##### Confetti -- Sparkling dots of light
* Settings
  * General Palettes -- See Palettes section below
---
##### Sinelon -- Sweeping bar of color with fading trails over height
* Settings
  * General Palettes -- See Palettes section below
  * Speed -- Adjusts the speed the bar moves
---
##### Beat -- Colored stripes pulsing at a defined Beats-Per-Minute
* Settings
  * General Palettes -- See Palettes section below
  * Speed -- Adjusts the speed the stripes moves
---
##### Juggle -- Several colored bars, weaving in and out of sync with each other
* Settings
  * Speed -- Adjusts the speed the bars move
---
##### TwinkleFOX -- Twinkling lights that randomly fade in and out from a palette
* Settings
  * Twinkle Palettes -- See Palettes section below
  * Speed -- Rate at which the twinkles fade in and out
  * Intensity -- Density of twinkles ranging from 0% to 100% coverage
---
##### Solid Color -- Fill all LEDs with the smae color
* Settings
  * Color Picker -- Sets the color

## Palettes
* Fire Palettes
    * Wood -- Orange
    * Sodium -- Yellow
    * Copper -- Green
    * Alcohol -- Blue
    * Rubidium -- Indigo
    * Potassium -- Violet
    * Lithium -- Red
    * Rotating -- ↻
---
* General Palettes
  * Rainbow -- Full spectrum
  * Rainbow Stripe -- Full spectrum with black stripes
  * Cloud -- Pale blues and whites
  * Lava -- Dark reds and black
  * Ocean -- Blues and greens
  * Forest -- Greens and yellows
  * Party -- Bright happy colors
  * Heat -- Reds, yellows and whites
---
* TwinkleFOX Palettes
  * Incandescent Twinkles
  * Red & White Twinkles
  * Blue & White Twinkles
  * Red, Green & White Twinkles
  * Fairy Light Twinkles
  * Snow 2 Twinkles
  * Holly Twinkles
  * Ice Twinkles
  * Party Twinkles
  * Forest Twinkles
  * Lava Twinkles
  * Fire Twinkles
  * Cloud 2 Twinkles
  * Ocean Twinkles
  * Rainbow Twinkles
  * Retro C9 Twinkles

Features
--------
* Turn LEDs on and off
* Adjust the brightness
* Change the display pattern
* Adjust the color
* Change color palettes
* Adjust individual pattern parameters

ToDo
--------
* Add MSGEQ7 patterns for XY matrix
* Add bleeding patterns
* Add scrolling text with input via web interface
* Save settings in array to allow each pattern to have their own settings
* Add reset for settings to default

Hardware
--------
To be updated

Web App
--------

![Web App](webapp.png)

Patterns are requested by the app from the ESP8266, so as new patterns are added, they're automatically listed in the app.

The web app is stored in SPIFFS (on-board flash memory).

The web app is a single page app that uses [jQuery](https://jquery.com) and [Bootstrap](http://getbootstrap.com).  It has buttons for On/Off, a slider for brightness, a pattern selector, and a color picker (using [jQuery MiniColors](http://labs.abeautifulsite.net/jquery-minicolors)).  Event handlers for the controls are wired up, so you don't have to click a 'Send' button after making changes.  The brightness slider and the color picker use a delayed event handler, to prevent from flooding the ESP8266 web server with too many requests too quickly.

The only drawback to SPIFFS that I've found so far is uploading the files can be extremely slow, requiring several minutes, sometimes regardless of how large the files are.  It can be so slow that I've been just developing the web app and debugging locally on my desktop (with a hard-coded IP for the ESP8266), before uploading to SPIFFS and testing on the ESP8266.

Installing
-----------
The app is installed via the Arduino IDE which can be [downloaded here](https://www.arduino.cc/en/main/software). The ESP8266 boards will need to be added to the Arduino IDE which is achieved as follows. Click File > Preferences and copy and paste the URL "http://arduino.esp8266.com/stable/package_esp8266com_index.json" into the Additional Boards Manager URLs field. Click OK. Click Tools > Boards: ... > Boards Manager. Find and click on ESP8266 (using the Search function may expedite this). Click on Install. After installation, click on Close and then select your ESP8266 board from the Tools > Board: ... menu.

The app depends on the following libraries. They must either be downloaded from GitHub and placed in the Arduino 'libraries' folder, or installed as [described here](https://www.arduino.cc/en/Guide/Libraries) by using the Arduino library manager.

* [FastLED](https://github.com/FastLED/FastLED)
* [Arduino WebSockets](https://github.com/Links2004/arduinoWebSockets)

Download the app code from GitHub using the green Clone or Download button from [the GitHub project main page](https://github.com/Intrinsically-Sublime/esp8266-fastled-webserver/tree/CCx_8266) and click Download ZIP. Decompress the ZIP file in your Arduino sketch folder.

The web app needs to be uploaded to the ESP8266's SPIFFS.  You can do this within the Arduino IDE after installing the [Arduino ESP8266FS tool](http://esp8266.github.io/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system).

With ESP8266FS installed upload the web app using `ESP8266 Sketch Data Upload` command in the Arduino Tools menu.

Then enter your wi-fi network SSID and password in the WiFi.h file, and upload the sketch using the Upload button.

Compression
-----------

The web app files can be gzip compressed before uploading to SPIFFS by running the following command:

`gzip -r data/`

The ESP8266WebServer will automatically serve any .gz file.  The file index.htm.gz will get served as index.htm, with the content-encoding header set to gzip, so the browser knows to decompress it.  The ESP8266WebServer doesn't seem to like the Glyphicon fonts gzipped, though, so I decompress them with this command:

`gunzip -r data/fonts/`

REST Web services
-----------------

The firmware implements basic [RESTful web services](https://en.wikipedia.org/wiki/Representational_state_transfer) using the ESP8266WebServer library.  Current values are requested with HTTP GETs, and values are set with POSTs using query string parameters.  It can run in connected or standalone access point modes.
