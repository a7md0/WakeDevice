# Wake Device [![Build Status](https://travis-ci.com/a7md0/WakeDevice.svg?branch=master)](https://travis-ci.com/a7md0/WakeDevice) [![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://github.com/a7md0/WakeDevice/blob/master/LICENSE)
Wake-On-LAN (WOL) packet forwarder, receive messages encoded in JSON via MQTT, and generate Wake-On-LAN packets based on the received MAC Address and send the WOL packet locally. Second approch is to check device status which send ICMP ping to device static IP and send message with the ping status.<br /><br />
This is intermediate device to acheive Wake-On-WAN without port forwarding. Moreover, This is solution for devices in networks behind firewall, [NAT](https://en.wikipedia.org/wiki/Network_address_translation) and/or [carrier-grade NAT](https://en.wikipedia.org/wiki/Carrier-grade_NAT). In most cases, ISPs does not provide public IP for home users.

# Requirements
* ESP32 module (Price range $5 to $10)
    * [Visual Studio Code](https://code.visualstudio.com/) (For programming the chip)
        * [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension
* [Amazon Web Services (AWS)](https://aws.amazon.com/) account
    * Amazon IoT [Free Tier - 12 MONTHS FREE] | MQTT Provider
    * Amazon Cognito [Free Tier - ALWAYS FREE] | Authentication
* Device with enabled Wake-On-LAN (WOL)
    * MAC Address (required)
    * Port 7 or 9 (required)
    * Static IP to retrieve device status via ICMP ping (optinal, supported)
    * SecureOn password (optional, supported)
* [Wake App](#-wake-app)

# Wake App
Application to add devices list and send message to wake/retrieve status. Built with Ionic 4 & Angular 8. Utilizing [AWS Amplify](https://aws-amplify.github.io/) for MQTT messaging.<br />
PWA Website => [Wake App](https://wakeapp.a7md0.dev/)<br />
Android => [Google Play](https://play.google.com/store/apps/details?id=dev.a7md0.wakeapp&hl=en)<br />
iOS => Not available at the moment

# Used libaries
[Arduino MQTT](https://github.com/256dpi/arduino-mqtt) by [256dpi](https://github.com/256dpi)<br />
[Arduino Json](https://github.com/bblanchon/ArduinoJson) by [bblanchon](https://github.com/bblanchon)<br />
[WakeOnLan](https://github.com/a7md0/WakeOnLan) by [a7md0](https://github.com/a7md0)<br />
[ESP32Ping](https://github.com/marian-craciunescu/ESP32Ping) by [Marian Craciunescu](https://github.com/marian-craciunescu)<br />
