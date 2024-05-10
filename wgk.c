/*
 * 
 *
 *
 *
 *
 */
 
 
// choose which you want. 
 
#undef  WEBSOCKET 
#define UDP 
#undef  ESP_NOW 

#include <Arduino.h>
#include <driver/i2s_tdm.h>
#include <WiFi.h>
#include <IPAddress.h>

// this is fixed for now but will be settable by config later. 
// WiFi credentials
const char* wifiSSID = "WGK"; 
const char* wifiPassword = "dQ.P2/xb1R!G6o+S";

#define SENDER 0
#define RECEIVER 1
int mode;


#ifdef WEBSOCKET 
#endif // WEBSOCKET

#ifdef UDP
#include <WiFiUdp.h>
const char* serverIP = "192.168.4.1"; // Receiver IP address
const int serverPort = 12345; // Port for UDP communication

// Define static IP configuration for the DAreceiver (SoftAP mode)
// will also be used for the setup function
IPAddress apIP(192.168.4.1); // Desired IP address for the Access Point
IPAddress gateway(192.168.4.1); // Gateway (usually same as IP)
IPAddress subnet(255.255.255.0); // Subnet mask
// Define static IP configuration for the sender (Station mode)
IPAddress staIP(192.168.4.2); // Desired static IP address
// use gateway and subnet from above

WiFiUdp udp;
#endif // UDP

#ifdef ESP_NOW
#include <esp_now.h>
// ESP-NOW encryption key (16-byte key)
// can be same as ssid passwd
const uint8_t* PMK = (uint8_t*) wifiPassword;
const uint8_t* LMK = PMK; // can be same for now. 
// MAC address of the receiver
uint8_t rxNumber = 1; // default value ; will be settable later. 
uint8_t rxMAC[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, rxNumber}; 
uint8_t txMAC[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 256 - rxNumber}; 
uint8_t wifiChan = rxNumber;
esp_now_peer_info_t peerInfo; 


#endif // ESP_NOW

// peripherals 
#define PIN_GKVOL 18
#define PIN_S1 19
#define PIN_S2 20

#define PIN_MCLK 2
#define PIN_BCLK GPIO_NUM_26
#define PIN DATA GPIO_NUM_25
#define PIN_WS GPIO_NUM_24
#define PIN_INT GPIO_NUM_23




#ifdef ESP_NOW
// Callback function for ESP-NOW data sent confirmation
// we will later wait to send a new packet until the old one was confirmed ? 
void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    Serial.print("Data sent to: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.print(" with status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}

// Callback function to handle received ESP-NOW messages
void onDataRecv(const uint8_t* mac_addr, const uint8_t* incomingData, int len) {
    Serial.print("Received data: ");
    for (int i = 0; i < len; i++) {
        Serial.print(incomingData[i]);
        Serial.print(" ");
    }
    Serial.println();
}
#endif // ESP_NOW


void setup() {
  Serial.begin(115200);

  // check sender or receiver using INT connected to WS
  // ... 
  mode = SENDER; 
  
  // check if we are not configured yet or setup button pressed 
  // if not, spawn AP and provide setup page
  // will not return but reset. 
  
  if (mode == RECEIVER) {

#ifdef UDP
    // Configure the AP with a static IP
    WiFi.softAPConfig(apIP, gateway, subnet);

    // Set up the Access Point with SSID and password
    WiFi.softAP(wifiSSID, wifiPassword);

    // Initialize UDP
    udp.begin(serverPort);
#endif
  
#ifdef ESP_NOW
    WiFi.mode(WIFI_STA);
    
    //Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
  
    esp_now_set_pmk((uint8_t *)PMK);

    // Once ESPNow is successfully Init, we will register for recv CB to
    // get recv packer info
    esp_now_register_recv_cb(OnDataRecv);
#endif

  
  
  } else {   // mode == SENDER
  
#ifdef UDP
    // Set static IP configuration before connecting to WiFi
    WiFi.config(staIP, gateway, subnet);

    // Connect to WiFi with the static IP configuration
    WiFi.begin(wifiSSID, wifiPassword)
#endif
  
#ifdef ESP_NOW
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_set_pmk((uint8_t *)PMK);
  
    esp_now_register_send_cb(OnDataSent);
   
    // register peer
    peerInfo.channel = rxNumber;  
    peerInfo.encrypt = false;
    // register first peer  
    memcpy(peerInfo.peer_addr, rxMAC, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
    }
#endif

  } // mode RX or TX



}


void loop() {

}






























