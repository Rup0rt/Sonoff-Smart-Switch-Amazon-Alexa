Code is taken from aneisch/Sonoff-Wemo-Home-Assistant

I fixed it to work with Amazon Alexa.
Tested with german version.

IMPORTANT: Newer versions of SONOFF devices, need flash mode DOUT in arduino IDE !!!

BLINK CODES:
============
* 2x long ON ==> SETUP MODE
   - LED goes ON ==> WPS DETECTION IN PROGRESS
   - LED goes OFF ==> WPS DETECTION FINISHED (you may restart the device manually the first time)
* Fast blinking ==> WLAN CONNECTION MODE
   - PRESS BUTTON while blinking ==> ENTER SETUP MODE
   - LED goes off after blinking ==> CONNECT SUCESSFULL
   - LED stays on after blinking ==> CONNECTION FAILED

Supported devices (tested):
Sonoff WIFI Smart Power socket
Sonoff WIFI Smart Switch

SONOFF TH16:
============

This device has a variable target temperature that is important when the current temperature is above or below this value. It can be changed on the internal webserver, that is reachable at http://[IP]:49153 and looks like this:
   
Tell Alexa to discover devices

Current temperature: 19.70 *C

Current humidity: 50.00 %

Target temperature: 23.50
Temperature: [            ] |CHANGE|
