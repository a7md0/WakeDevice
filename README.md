# Wake Device [![Build Status](https://travis-ci.com/a7md0/WakeDevice.svg?branch=master)](https://travis-ci.com/a7md0/WakeDevice) [![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://github.com/a7md0/WakeDevice/blob/master/LICENSE)
This is an intermediate device to achieve Wake-On-WAN without port forwarding, this is a solution for devices in networks behind a firewall, [NAT](https://en.wikipedia.org/wiki/Network_address_translation) and/or [carrier-grade NAT](https://en.wikipedia.org/wiki/Carrier-grade_NAT). In most cases, ISPs does not provide public IP for home users.
<br /><br />
Wake-On-LAN (WOL) packet forwarder, receive messages encoded in JSON via MQTT, and generate WakeOnLAN packets based on the received MAC Address and send the WOL packet to the Local-Area-Network. The second approach is to check device status which runs ICMP ping to device static IP and send-back message with the ping status.

# Requirements
* ESP32 module
    * [Visual Studio Code](https://code.visualstudio.com/) (For programming the chip)
        * [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension
* Account from [Amazon Web Services (AWS)](https://aws.amazon.com/) 
    * MQTT Provider: Amazon IoT [Free Tier - 12 MONTHS FREE]
    * Authentication: Amazon Cognito [Free Tier - ALWAYS FREE]
* [Wake App](#wake-app)

# Device
* MAC Address (required)
* Wake-On-LAN enabled (required)
* Deafult to port 9 (port 7 supported)
* Static IP to retrieve device status via ICMP ping (optional, supported)
* SecureOn password (optional, supported)

# Wake App
Application to add devices list and send message to wake/retrieve status. Built with Ionic 4 & Angular 8. Utilizing [AWS Amplify](https://aws-amplify.github.io/) for MQTT messaging.<br /><br />
Website: [Wake App](https://wakeapp.a7md0.dev/)<br />
Android: [Google Play - Wake App](https://play.google.com/store/apps/details?id=dev.a7md0.wakeapp&hl=en)<br />
iOS: Not available at the moment

# Libraries
[Arduino MQTT](https://github.com/256dpi/arduino-mqtt) by [256dpi](https://github.com/256dpi)<br />
[Arduino Json](https://github.com/bblanchon/ArduinoJson) by [bblanchon](https://github.com/bblanchon)<br />
[WakeOnLan](https://github.com/a7md0/WakeOnLan) by [a7md0](https://github.com/a7md0)<br />
[ESP32Ping](https://github.com/marian-craciunescu/ESP32Ping) by [Marian Craciunescu](https://github.com/marian-craciunescu)<br />

# License
Wake Device
Copyright (C) 2019 Ahmed Al-Qaidom

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
