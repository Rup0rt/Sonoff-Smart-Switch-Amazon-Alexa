Code is taken from aneisch/Sonoff-Wemo-Home-Assistant

I fixed it to work with Amazon Alexa.
Tested with german version.

IMPORTANT: Newer versions of SONOFF devices, need flash mode DOUT in arduino IDE !!!

BLINK CODES:
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
