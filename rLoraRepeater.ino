#include <RadioLib.h>
#include <Wire.h>
#include <U8g2lib.h>

// Pin definitions for Heltec WiFi LoRa 32 V3
const int pin_cs = 8;
const int pin_busy = 13;
const int pin_dio = 14;
const int pin_reset = 12;
const int pin_mosi = 10;
const int pin_miso = 11;
const int pin_sclk = 9;
const int pin_led_rx = 35;
const int pin_btn_usr1 = 0;

// LoRa parameters - PROVEN WORKING CONFIGURATION
const float FREQUENCY = 869.525;
const int SPREADING_FACTOR = 7;
const int CODING_RATE = 5;
const float BANDWIDTH = 125.0;
const uint8_t SYNC_WORD = 0x42;
const int PREAMBLE_LENGTH = 12;
const bool CRC_ENABLED = true;

// Display setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, 21, 18, 17);

// SX1262 radio setup
SX1262 radio = new Module(pin_cs, pin_dio, pin_reset, pin_busy);

// Global Statistics Variables
int totalPackets = 0;
int repeatedPackets = 0;
int announcePackets = 0;
int pathPackets = 0;
int dataPackets = 0;
int unknownPackets = 0;
int lastRSSI = 0;
float lastSNR = 0;
bool radioOK = false;
unsigned long lastPacketTime = 0;

// State control to prevent loops
bool processingPacket = false;
unsigned long lastTransmitTime = 0;
const unsigned long ANTI_LOOP_DELAY = 2000;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== RNODE PACKET ANALYZER + REPEATER ===");
  Serial.println("Anti-Loop Version with Repeater Function");
  Serial.println("Using PROVEN working configuration:");
  Serial.printf("SW: 0x%02X, Preamble: %d, CRC: %s\n", SYNC_WORD, PREAMBLE_LENGTH, CRC_ENABLED ? "ON" : "OFF");
  Serial.println("Mode: ANALYZE + REPEAT");
  Serial.println("=========================================");
  
  initDisplay();
  initRadio();
  
  if (radioOK) {
    Serial.println("Packet analyzer + repeater ready!");
    Serial.println("Listening for Reticulum traffic...");
    Serial.println("=============================");
  }
}

void initDisplay() {
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);
  delay(100);
  digitalWrite(21, HIGH);
  delay(100);
  
  if (display.begin()) {
    // Splash screen intro
    display.clearBuffer();
    display.setFont(u8g2_font_10x20_tf);  // Larger font for title
    int titleWidth = display.getStrWidth("rLoraRepeater");
    int centerX = (128 - titleWidth) / 2;  // Center on 128px display
    display.drawStr(centerX, 35, "rLoraRepeater");
    display.setFont(u8g2_font_6x10_tf);   // Smaller font for version
    int versionWidth = display.getStrWidth("v1.0");
    int versionX = (128 - versionWidth) / 2;
    display.drawStr(versionX, 50, "v1.0");
    display.sendBuffer();
    
    delay(3000);  // Show splash for 2 seconds
    
    // Clear and show startup info
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 10, "rLoraRepeater v1.0");
    display.drawStr(0, 22, "Broadcast Version");
    display.drawStr(0, 34, "SW:0x42 P:12 CRC:On");
    display.drawStr(0, 46, "Ready To Listen!");
    display.sendBuffer();
  }
}

void initRadio() {
  pinMode(pin_led_rx, OUTPUT);
  pinMode(pin_btn_usr1, INPUT_PULLUP);
  digitalWrite(pin_led_rx, LOW);
  
  // Hardware reset
  pinMode(pin_reset, OUTPUT);
  digitalWrite(pin_reset, LOW);
  delay(200);
  digitalWrite(pin_reset, HIGH);
  delay(300);
  
  // Reliable SPI settings
  SPI.begin(pin_sclk, pin_miso, pin_mosi);
  SPI.setFrequency(1000000);
  
  Serial.print("Initializing radio... ");
  if (radio.begin() == RADIOLIB_ERR_NONE) {
    Serial.println("OK");
    
    // Apply PROVEN WORKING configuration
    radio.setFrequency(FREQUENCY);
    radio.setBandwidth(BANDWIDTH);
    radio.setSpreadingFactor(SPREADING_FACTOR);
    radio.setCodingRate(CODING_RATE);
    radio.setSyncWord(SYNC_WORD);
    radio.setPreambleLength(PREAMBLE_LENGTH);
    radio.setCRC(CRC_ENABLED);
    radio.setDio2AsRfSwitch(true);
    radio.explicitHeader();
    radio.setOutputPower(14); // Set to 2 dBm minimum power, default is 14dbm
    Serial.printf("Applied working config: SW=0x%02X, Preamble=%d\n", SYNC_WORD, PREAMBLE_LENGTH);
    
    delay(100);
    
    // Start with a fresh receive state
    startFreshReceive();
    radioOK = true;
    
  } else {
    Serial.println("FAILED");
    while(1) delay(1000);
  }
}

void startFreshReceive() {
  Serial.println("DEBUG: Starting fresh receive mode...");
  radio.standby();
  delay(50);
  radio.sleep();
  delay(50);
  radio.standby();
  delay(50);
  
  int result = radio.startReceive();
  if (result == RADIOLIB_ERR_NONE) {
    Serial.println("DEBUG: Fresh receive mode started successfully");
    processingPacket = false;
  } else {
    Serial.printf("DEBUG: Failed to start receive: %d\n", result);
  }
}

void loop() {
  if (!radioOK) return;
  
  // Button reset
  if (digitalRead(pin_btn_usr1) == LOW) {
    delay(50);
    if (digitalRead(pin_btn_usr1) == LOW) {
      Serial.println("=== MANUAL RESET ===");
      radioOK = false;
      delay(500);
      initRadio();
      updateDisplay();
      
      while (digitalRead(pin_btn_usr1) == LOW) delay(10);
      delay(1000);
    }
  }
  
  // Only check for packets if we're not already processing one
  if (!processingPacket) {
    
    // Check if packet is available (non-blocking)
    int availableLength = radio.getPacketLength();
    
    if (availableLength > 0) {
      Serial.printf("DEBUG: Packet detected - %d bytes available\n", availableLength);
      processingPacket = true; // Lock to prevent re-entry
      
      uint8_t buffer[256];
      memset(buffer, 0, sizeof(buffer));
      
      // Read the packet
      int state = radio.readData(buffer, sizeof(buffer));
      
      if (state == RADIOLIB_ERR_NONE) {
        // Get actual packet length after reading
        int packetLength = availableLength;
        
        Serial.printf("DEBUG: Successfully read %d bytes\n", packetLength);
        
        totalPackets++;
        lastPacketTime = millis();
        lastRSSI = radio.getRSSI();
        lastSNR = radio.getSNR();
        
        // Visual feedback
        digitalWrite(pin_led_rx, HIGH);
        delay(100);
        digitalWrite(pin_led_rx, LOW);
        
        // Analyze the packet
        analyzePacket(buffer, packetLength);
        
        // REPEAT THE PACKET
        Serial.println("=== REPEATING PACKET ===");
        repeatPacket(buffer, packetLength);
        
        updateDisplay();
        
        Serial.println("=== PACKET ANALYSIS COMPLETE ===");
        
      } else {
        Serial.printf("DEBUG: Read error: %d\n", state);
      }
      
      // CRITICAL: Complete radio reset after packet AND transmission
      Serial.println("DEBUG: Resetting radio after repeat...");
      delay(500); // Wait for transmission to complete
      
      startFreshReceive(); // Complete reset and restart
      
      Serial.println("=== READY FOR NEXT PACKET ===\n");
      
      // Extended delay to prevent loops after transmission
      lastTransmitTime = millis();
      delay(3000); // Longer delay after repeating
      
    } else {
      // No packet available - this is the normal case
      // Don't print debug messages here to avoid spam
    }
  }
  
  delay(100); // Main loop delay
}

void repeatPacket(uint8_t* packet, int length) {
  Serial.printf("Repeating %d byte packet...\n", length);
  
  // Anti-collision delay before transmitting
  delay(random(100, 500)); // Random delay to avoid collisions
  
  // Switch to transmit mode
  radio.standby();
  delay(50);
  
  // Transmit the packet exactly as received
  int state = radio.transmit(packet, length);
  
  if (state == RADIOLIB_ERR_NONE) {
    repeatedPackets++;
    Serial.printf("SUCCESS: Packet repeated! (Total repeated: %d)\n", repeatedPackets);
    
    // Visual feedback for transmission
    for (int i = 0; i < 3; i++) {
      digitalWrite(pin_led_rx, HIGH);
      delay(100);
      digitalWrite(pin_led_rx, LOW);
      delay(100);
    }
    
  } else {
    Serial.printf("FAILED: Repeat error %d\n", state);
  }
  
  Serial.println("=== REPEAT COMPLETE ===");
}

void analyzePacket(uint8_t* packet, int length) {
  Serial.printf("\n================= PACKET #%d =================\n", totalPackets);
  Serial.printf("Timestamp: %lu ms\n", millis());
  Serial.printf("Length: %d bytes\n", length);
  Serial.printf("RSSI: %d dBm, SNR: %.2f dB\n", lastRSSI, lastSNR);
  Serial.println();
  
  // RAW LOG - Complete packet as comma-separated hex values
  Serial.println("=== RAW LOG (Copy/Paste Ready) ===");
  Serial.print("RAW: ");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X", packet[i]);
    if (i < length - 1) Serial.print(",");
  }
  Serial.println();
  
  // Alternative format - continuous hex string
  Serial.print("HEX: ");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X", packet[i]);
  }
  Serial.println();
  Serial.println();
  
  // Full hex dump
  Serial.println("=== FULL HEX DUMP ===");
  for (int i = 0; i < length; i++) {
    if (i % 16 == 0) {
      Serial.printf("%04X: ", i);
    }
    Serial.printf("%02X ", packet[i]);
    if ((i + 1) % 16 == 0 || i == length - 1) {
      // Fill remaining space for alignment
      for (int j = (i % 16) + 1; j < 16; j++) {
        Serial.print("   ");
      }
      Serial.print(" | ");
      // ASCII representation
      int start = i - (i % 16);
      for (int j = start; j <= i; j++) {
        if (packet[j] >= 32 && packet[j] <= 126) {
          Serial.printf("%c", packet[j]);
        } else {
          Serial.print(".");
        }
      }
      Serial.println();
    }
  }
  Serial.println();
  
  // Packet structure analysis
  Serial.println("=== PACKET STRUCTURE ANALYSIS ===");
  
  // First few bytes (header analysis)
  Serial.print("Header bytes: ");
  for (int i = 0; i < min(8, length); i++) {
    Serial.printf("0x%02X ", packet[i]);
  }
  Serial.println();
  
  if (length >= 2) {
    Serial.printf("First byte: 0x%02X (%d) - ", packet[0], packet[0]);
    analyzeByte(packet[0]);
    Serial.printf("Second byte: 0x%02X (%d) - ", packet[1], packet[1]);
    analyzeByte(packet[1]);
  }
  
  // Look for patterns
  Serial.println("\n=== PATTERN ANALYSIS ===");
  
  // Check for common Reticulum patterns
  String packetType = "UNKNOWN";
  bool hasNullBytes = false;
  bool hasHighBytes = false;
  int nullCount = 0;
  
  // Analyze byte patterns
  for (int i = 0; i < length; i++) {
    if (packet[i] == 0x00) {
      nullCount++;
      hasNullBytes = true;
    }
    if (packet[i] > 0x7F) {
      hasHighBytes = true;
    }
  }
  
  // Heuristic packet type detection
  if (length > 20) {
    if (length > 50 && nullCount < 5 && hasHighBytes) {
      packetType = "POSSIBLE_ANNOUNCE";
      announcePackets++;
    } else if (length < 30 && nullCount > 2) {
      packetType = "POSSIBLE_ROUTING";
      pathPackets++;
    } else if (length > 30 && length < 100) {
      packetType = "POSSIBLE_DATA";
      dataPackets++;
    } else {
      unknownPackets++;
    }
  } else {
    packetType = "SHORT_PACKET";
    unknownPackets++;
  }
  
  Serial.printf("Detected type: %s\n", packetType.c_str());
  Serial.printf("Null bytes: %d/%d\n", nullCount, length);
  Serial.printf("High bytes (>0x7F): %s\n", hasHighBytes ? "YES" : "NO");
  
  // Entropy analysis (rough)
  int entropy = calculateEntropy(packet, length);
  Serial.printf("Rough entropy: %d%% (higher = more random/encrypted)\n", entropy);
  
  // Look for ASCII strings
  Serial.println("\n=== ASCII STRING SEARCH ===");
  findAsciiStrings(packet, length);
  
  // End markers analysis
  Serial.println("\n=== END ANALYSIS ===");
  Serial.print("Last 16 bytes: ");
  for (int i = max(0, length-16); i < length; i++) {
    Serial.printf("%02X ", packet[i]);
  }
  Serial.println();
  
  // Look for readable text at end
  Serial.print("End as text: ");
  for (int i = max(0, length-16); i < length; i++) {
    if (packet[i] >= 32 && packet[i] <= 126) {
      Serial.printf("%c", packet[i]);
    } else {
      Serial.print(".");
    }
  }
  Serial.println();
  
  Serial.println("===============================================\n");
}

void analyzeByte(uint8_t byte) {
  Serial.print("Binary: ");
  for (int i = 7; i >= 0; i--) {
    Serial.print((byte >> i) & 1);
  }
  Serial.println();
}

int calculateEntropy(uint8_t* data, int length) {
  int freq[256] = {0};
  
  // Count frequency of each byte
  for (int i = 0; i < length; i++) {
    freq[data[i]]++;
  }
  
  // Count unique bytes
  int uniqueBytes = 0;
  for (int i = 0; i < 256; i++) {
    if (freq[i] > 0) uniqueBytes++;
  }
  
  // Simple entropy measure: percentage of unique bytes
  return (uniqueBytes * 100) / 256;
}

void findAsciiStrings(uint8_t* packet, int length) {
  String currentString = "";
  bool inString = false;
  
  for (int i = 0; i < length; i++) {
    if (packet[i] >= 32 && packet[i] <= 126) { // Printable ASCII
      currentString += (char)packet[i];
      if (!inString) {
        inString = true;
      }
    } else {
      if (inString && currentString.length() >= 4) { // Min 4 chars
        Serial.printf("ASCII string at pos %d: \"%s\"\n", i - currentString.length(), currentString.c_str());
      }
      currentString = "";
      inString = false;
    }
  }
  
  // Check final string
  if (inString && currentString.length() >= 4) {
    Serial.printf("ASCII string at end: \"%s\"\n", currentString.c_str());
  }
  
  if (!inString && currentString.length() == 0) {
    Serial.println("No significant ASCII strings found");
  }
}

void updateDisplay() {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  
  display.drawStr(0, 10, "Analyzer+Repeater");
  
  char buffer[32];
  sprintf(buffer, "RX:%d TX:%d", totalPackets, repeatedPackets);
  display.drawStr(0, 22, buffer);
  
  sprintf(buffer, "A:%d R:%d D:%d U:%d", announcePackets, pathPackets, dataPackets, unknownPackets);
  display.drawStr(0, 34, buffer);
  
  if (totalPackets > 0) {
    sprintf(buffer, "RSSI:%d SNR:%.1f", lastRSSI, lastSNR);
    display.drawStr(0, 46, buffer);
    
    unsigned long timeSince = (millis() - lastPacketTime) / 1000;
    sprintf(buffer, "Last: %lus ago", timeSince);
    display.drawStr(0, 58, buffer);
  } else {
    display.drawStr(0, 46, "Listening...");
    display.drawStr(0, 58, "869.525 MHz");
  }
  
  display.sendBuffer();
}