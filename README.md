# rLoraRepeater v1.0


# Reticulum LoRa Standalone Packet Analyzer and Broadcast Repeater

A comprehensive standalone packet analyzer (from serial output monitor) and broadcast repeater (read: announces repeater) for Reticulum mesh networks using Heltec WiFi LoRa 32 V3 hardware.

## Features

- **Packet Analysis**: Comprehensive analysis of Reticulum packets including hex dumps, header parsing, and protocol detection via serial output (eg. arduino serial monitor)
- **Broadcast Repeating**: Reliable repeating of announce packets and broadcast traffic to extend network coverage
- **Protocol Insight**: Detailed logging of packet structure, entropy analysis, ASCII string detection, and node identification
- **Anti-Loop Protection**: Robust timing mechanisms prevent infinite packet loops
- **Real-time Display**: OLED display showing packet statistics, signal strength, and operation status
- **Serial Logging**: Complete packet analysis with raw hex dumps for protocol research

## What It Does

This firmware captures and analyzes Reticulum packets transmitted over LoRa, specifically designed to listen RNode packets, providing detailed insight into:
- Node announces and identity advertisements 
- Packet structure and routing headers  
- Signal quality (RSSI/SNR) and network topology
- Different announcement types (user nodes, peer, services, BBS systems, unknown types)

The repeater function extends network coverage by retransmitting broadcast packets (reticulum announces), enabling nodes that cannot communicate directly to receive announces and network discovery information.

## What It Doesn't Do

This is a **broadcast-only repeater** that handles one-way traffic:
- ✅ Announces, network discovery, topology information
- ❌ Bidirectional links, messaging, file transfers
- ❌ Full transport node functionality

For complete mesh routing capabilities, a full Reticulum transport node implementation would be required.

## Hardware Requirements

- **Heltec WiFi LoRa 32 V3** development board
- Built-in SX1262 LoRa radio (869.525 MHz configuration, personalize to your preferences in the arduino sketch!)
- Built-in OLED display
- USB connection for programming and serial monitoring

## Software Requirements

- Arduino IDE 1.8.x or 2.x
- ESP32 Board Support Package
- Required Libraries:
  - `RadioLib` by Jan Gromeš
  - `U8g2` by Oliver Kraus
  - `Wire` library (built-in)

## Installation

## METHOD A, USING ARDUINO IDE:

### 1. Install Arduino IDE and ESP32 Support

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support:
   - File → Preferences
   - Add to Additional Board Manager URLs: `https://dl.espressif.com/dl/package_esp32_index.json`
   - Tools → Board → Board Manager
   - Search "ESP32" and install "esp32 by Espressif Systems"

### 2. Install Required Libraries

Open Arduino IDE Library Manager (Tools → Manage Libraries) and install:
- **RadioLib** (search "RadioLib")
- **U8g2** (search "U8g2")

### 3. Configure Board Settings

1. Tools → Board → ESP32 Arduino → "Heltec WiFi LoRa 32(V3)"
2. Tools → Upload Speed → 115200
3. Tools → CPU Frequency → 240MHz
4. Tools → Flash Frequency → 80MHz
5. Tools → Partition Scheme → "Default 4MB..."

### 4. Flash the Firmware

1. Connect Heltec V3 via USB
2. Select correct COM/Serial port in Tools → Port
3. Open the firmware sketch
4. Click Upload button or Ctrl+U

## METHOD B, FLASHING PREMADE FIRMWARE WITH ESP32FLASHTOOL

### Using Precompiled Binary (No Arduino IDE Required)
For users who want to flash the firmware without setting up the development environment:

### Download the precompiled firmware:

Go to the Releases page

Download rLoraRepeater_v1.0.bin


### Install ESP32 Flash Tool:

Download ESP32 Flash Download Tool from Espressif

Or install esptool: pip install esptool


### Flash using ESP32 Flash Download Tool:

Connect your Heltec V3 via USB

Open ESP32 Flash Download Tool

Select "ESP32" and "Develop" mode

Add the firmware file:

File path: Browse to rLoraRepeater_v1.0.bin

Address: 0x10000


Select your COM port and set baud rate to 115200

Click "START" to flash


### Flash using esptool (Command Line):

(Replace COM3 with your actual port)

esptool.py --chip esp32 --port COM3 --baud 115200 write_flash 0x10000 rLoraRepeater_v1.0.bin



-----


## Configuration

The firmware uses proven working LoRa parameters for Reticulum compatibility, you can edit your lora parameters to meet your RNode settings:

```cpp
const float FREQUENCY = 869.525;      // MHz
const int SPREADING_FACTOR = 7;
const int CODING_RATE = 5;
const float BANDWIDTH = 125.0;        // kHz
const uint8_t SYNC_WORD = 0x42;
const int PREAMBLE_LENGTH = 12;
const bool CRC_ENABLED = true;
```

To modify transmission power for testing:
```cpp
radio.setOutputPower(2); // Set to 2 dBm minimum power, default is 14dbm without this string
```

## Usage

### Basic Operation

1. Power on the device
2. Monitor serial output at 115200 baud for packet analysis (optional, the node can run standalone without serial output info, just plug the battery!)
3. Watch OLED display for real-time statistics
4. Press the user button to manually reset the radio (prg button)

### Testing Network Extension

To verify the repeater extends network coverage:

1. Set up Node A (RNode with Reticulum) and Node C (test receiver) at opposite ends of coverage area
2. Ensure they cannot communicate directly
3. Place the repeater between them
4. Observe that Node C begins receiving announces from Node A via the repeater

### Serial Output

The firmware provides comprehensive packet analysis via serial console:

```
================= PACKET #15 =================
Timestamp: 245780 ms
Length: 201 bytes
RSSI: -45 dBm, SNR: 8.25 dB

=== RAW LOG (Copy/Paste Ready) ===
RAW: 30,51,03,C2,9C,B0,CB,21,72,02,31,43,23,30...
HEX: 305103C29CB0CB217202314323309C5F9AEDDF...

=== FULL HEX DUMP ===
0000: 30 51 03 C2 9C B0 CB 21 72 02 31 43 23 30 9C 5F  | 0Q.....!r.1C#0._
[...full packet dump...]

=== PATTERN ANALYSIS ===
Detected type: POSSIBLE_ANNOUNCE
ASCII string at end: "Sydney RNS Node"
```

### Display Information

The OLED shows:
- Line 1: "Analyzer+Repeater"  
- Line 2: RX:nn TX:nn (received/transmitted packet counts)
- Line 3: A:n R:n D:n U:n (announce/routing/data/unknown packet types)
- Line 4: Signal strength (RSSI/SNR of last packet)
- Line 5: Time since last packet or current status

## Network Compatibility

This repeater is compatible with standard Reticulum networks using:
- 869.525 MHz frequency (editable)
- LoRa modulation with SF7, BW125, CR4/5 (editable)
- Sync word 0x42, Preamble length 12 (fixed for reticulum packets reception)
- Standard Reticulum packet framing

## Troubleshooting

**No packets received:**
- Verify LoRa parameters match your Reticulum network
- Check antenna connections
- Ensure there's active Reticulum traffic in range

**Packet reception but no repeating:**
- Check serial output for "SUCCESS: Packet repeated" messages
- Verify TX power setting allows transmission
- Ensure adequate power supply



## Development

### Protocol Research

The packet analyzer provides valuable data for Reticulum protocol research:
- Raw packet dumps for external analysis
- Header structure identification  
- Node naming convention patterns
- Network topology discovery

### Extending Functionality

To add features:
- Modify `analyzePacket()` for additional packet analysis
- Extend routing logic in the main loop
- Add filtering based on packet types or sources

## Known Limitations

- Broadcast-only repeating (working announces only, no bidirectional link support)
- No packet filtering or selective forwarding
- Fixed LoRa parameters (no dynamic configuration, edit the sketch to modify LoRa settings!)
- Memory constraints limit routing table capabilities

The limits of transparent/dumb repeaters in Reticulum networks are significant and stem from the protocol's design as an intelligent mesh system rather than simple RF amplification:

Protocol-Level Limitations:

Bidirectional Communication Failure:

    Link establishment requires handshake timing that repeaters disrupt
    Acknowledgment packets must follow specific timing windows
    Session keys and sequence numbers become desynchronized

Path Discovery Issues:

    Reticulum uses cryptographic path proofs that can't be blindly repeated
    Hop count management requires protocol awareness
    Route metrics become inaccurate without proper path calculation

Flooding and Loops:

    Without routing intelligence, announces can create broadcast storms
    No understanding of network topology leads to inefficient propagation
    Duplicate detection relies on protocol-level sequence tracking

Security Implications:

    Cannot validate packet authenticity or integrity
    May propagate malicious or corrupted packets
    Bypasses Reticulum's built-in security mechanisms

What Actually Works:

    One-way announcements (network discovery)
    Broadcast packets without response requirements
    Simple range extension for topology awareness

Why This Matters: Reticulum was designed assuming intelligent nodes that understand routing, cryptography, and session management. Dumb repeaters break these assumptions, announces working but messaging failing.

The Fundamental Problem: Transparent repeaters treat Reticulum like a simple radio protocol when it's actually a sophisticated networking stack. This mismatch creates the limitations you can observe with a firmware like this.

The transparent or dumb repeater succeeds where Reticulum behavior resembles broadcast radio, but fails where it requires network intelligence.

# THIS IS AN EXPERIMENTAL PROJECT AND MAY BE REMAIN AS IT IS, IF YOU CAN, CONTRIBUTE TO IT IMPLEMENTING RETICULUM STACK TRANSPORT INSIDE THE FIRMWARE.

## License

[MIT]


## Contributing

Contributions welcome! Please submit issues and pull requests for:
- Additional packet analysis features
- Protocol compatibility improvements  
- Hardware platform support
- Documentation enhancements
- Retiulum stack implementation
- Reticulum Transport implementation
- Routing / Link / Path implementation
- 
## Related Projects

- [Reticulum Network Stack](https://github.com/markqvist/Reticulum) - The complete mesh networking protocol
- [RNode Firmware](https://github.com/markqvist/RNode_Firmware) - Official RNode radio interface firmware  
- [microReticulum](https://github.com/attermann/microReticulum) - C++ port for standalone nodes

## Acknowledgments

Built on the excellent work of:
- Mark Qvist - Reticulum Network Stack and RNode platform
- Jan Gromeš - RadioLib LoRa library
- Oliver Kraus - U8g2 display library
