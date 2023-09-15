This is for ESP32 only

InstallationId will be the MAC address. 

In ParseClient class, I manually add "/parse" to request(because my Parse address is http://parseser.com:1337/parse), if your Parse Server URL doesn't have this, please modify the sendRequest function.

It's based on this https://github.com/parse-community/Parse-SDK-Arduino
