**Reflect** is a hardware AI Assistant that was built during a OpenAI hackathon.
Currently it is built to target espressif devices, but we hope to expand
its capabilites if users find it useful. The following were some of the
ideas/inspirations during the project.

<hr />

<table>
  <tr>
    <td width="50%" valign="top">
      <img src="./.github/reflect.png" height="650px" alt="Reflect">
    </td>
    <td width="50%" valign="top">
      <b> Communicate naturally </b>
      <ul>
        <li>sound, light and color, avoid screens</li>
      </ul>
      <b> Phone is your key </b>
      <ul>
        <li>device has no state, all info is on phone</li>
      </ul>
      <b> Reflect on yesterday </b>
      <ul>
        <li>ask about events in your calendar</li>
      </ul>
      <b> Prepare for tomorrow </b>
      <ul>
        <li>you have a test tomorrow, want to study?</li>
      </ul>
      <b> Assistant to keep you in flow</b>
      <ul>
        <li>play music while studying, ask quick questions</li>
      </ul>
      <b>Location Aware</b>
      <ul>
        <li>unique behavior in kitchen/office...</li>
      </ul>
      <b>Designed to modify</b>
      <ul>
        <li>should be easy to modify/try new things</li>
      </ul>
      <b>Cheap</b>
      <ul>
        <li>make it available to as many as possible</li>
      </ul>
    </td>
  </tr>
</table>

### Supported Devices
We hope to expand this list. Reflect was written for a specific device to start, but if it is useful we want to add more.

#### Microcontrollers
* [M5Stack CoreS3 ESP32S3 loT Development Kit](https://shop.m5stack.com/products/m5stack-cores3-esp32s3-lotdevelopment-kit)

#### Lights
* [LIFX Color A19](https://www.amazon.com/dp/B08BKZFHQQ)

### Installing



##### Setup esp-idf
```
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ~/esp-idf/export.sh
```

### Install Code to Device
```
. ~/esp-idf/export.sh
idf.py flash
```

### Device with running code
<video src="https://github.com/user-attachments/assets/d56fcb7a-5807-43f1-b314-070e2629bd39" controls />

### Using
The device creates a WiFi Access Point named `reflect`. Connect to and it and
open http://192.168.4.1 to start a session.
