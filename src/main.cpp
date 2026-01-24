#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <Preferences.h> 
#include "cc_interface.h"
#include "web_index.h" 
#include "flasher_controller.h"
#include "web_js.h"
#include "web_lang.h"

// --- HARDWARE CONFIG ---
#define PIN_CC_CLK  4
#define PIN_CC_DATA 5
#define PIN_CC_RST  6

// --- WIFI DEFAULT (Hotspot) ---
const char* ap_ssid       = "ESP32-CC-Flasher";
const char* ap_password   = "12345678";

AsyncWebServer server(80);

// --- GLOBAL VARIABLES ---
Preferences preferences; 
bool isApMode = false;

void setup() {
    Serial.begin(115200);
    
    // 1. Controller Init
    initFlasherController();

    if(!LittleFS.begin(true)){ Serial.println("FS Fail"); return; }

    uint16_t id = cc.begin(PIN_CC_CLK, PIN_CC_DATA, PIN_CC_RST);
    Serial.printf("CC-ID: 0x%04X\n", id);

    // --- WIFI LOGIC ---
    preferences.begin("wifi-config", false); // Open Namespace
    String ssid = preferences.getString("ssid", ""); 
    String pass = preferences.getString("pass", "");

    WiFi.mode(WIFI_STA);
    
    if(ssid.length() > 0) {
        Serial.println("Connecting to: " + ssid);
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        // Wait max 10 seconds
        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 20) { 
            delay(500); 
            Serial.print("."); 
            timeout++; 
        }
    }

    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
        isApMode = false;
    } else {
        Serial.println("\nNo Connection. Starting AP Mode.");
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP(ap_ssid, ap_password);
        Serial.println("AP IP: " + WiFi.softAPIP().toString());
        isApMode = true;
    }

    MDNS.begin("cc-tool");

    // --- WEB ROUTES ---

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(200, "text/html", index_html); });

    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/javascript", javaScript);
    });

    server.on("/api/system_ip", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(200, "text/plain", (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
    });

    // API: System Info & Mode
    server.on("/api/system_info", HTTP_GET, [](AsyncWebServerRequest *r){
        String json = "{";
        json += "\"ip\":\"" + (isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
        json += "\"mode\":\"" + String(isApMode ? "AP" : "STA") + "\""; 
        json += "}";
        r->send(200, "application/json", json);
    });

    server.on("/api/lang", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", languages_json);
    });

    // API: Save WiFi credentials
    server.on("/api/save_wifi", HTTP_POST, [](AsyncWebServerRequest *r){
        if(r->hasParam("ssid", true) && r->hasParam("pass", true)) {
            String new_ssid = r->getParam("ssid", true)->value();
            String new_pass = r->getParam("pass", true)->value();
            
            preferences.putString("ssid", new_ssid);
            preferences.putString("pass", new_pass);
            
            r->send(200, "text/plain", "Saved. Restarting...");
            delay(1000);
            ESP.restart(); 
        } else {
            r->send(400, "text/plain", "Missing data");
        }
    });

    server.on("/api/init", HTTP_GET, [](AsyncWebServerRequest *request){
        cc.enable_cc_debug(); cc.clock_init();
        request->send(200, "text/plain", "Init OK");
    });

    server.on("/api/pins", HTTP_GET, [](AsyncWebServerRequest *r){
        String json = "{";
        json += "\"clk\":" + String(PIN_CC_CLK) + ",";
        json += "\"data\":" + String(PIN_CC_DATA) + ",";
        json += "\"rst\":" + String(PIN_CC_RST);
        json += "}";
        r->send(200, "application/json", json);
    });

    // Info Block
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request){
        uint16_t raw_id = cc.send_cc_cmd(0x68);
        uint8_t chip_id = (raw_id >> 8) & 0xFF; 
        uint8_t chip_rev = raw_id & 0xFF;
        
        String modelName = "Unknown (0x" + String(chip_id, HEX) + ")";
        String flashSize = "Unknown"; 
        bool hasMac = true; 
        bool isLocked = false;
        
        String rawDump = ""; 
        uint8_t infoBuf[8]; 
        bool allZeros = true;
        
        cc.WR_CONFIG(0x01); 
        cc.read_xdata_memory(0x0000, 8, infoBuf); 
        cc.WR_CONFIG(0x00); 
        
        for(int i=0; i<8; i++){ 
            if(infoBuf[i] != 0x00) allZeros = false; 
            if(infoBuf[i]<0x10) rawDump+="0"; 
            rawDump+=String(infoBuf[i], HEX)+" "; 
        }
        rawDump.toUpperCase();

        if((chip_id == 0x11 || chip_id == 0x01) && allZeros) {
            isLocked = true;
            flashSize = "Locked (Protected)";
            modelName += " [LOCKED]";
        }

        if(chip_id == 0x01 || chip_id == 0x11) {
            modelName = (chip_id == 0x11) ? "CC1111 (USB)" : "CC1110"; hasMac = false;
            if(!isLocked) flashSize = String(cc.detect_flash_size() / 1024) + " KB (Detected)";
        } else if(chip_id == 0xA5) { modelName = "CC2530"; hasMac = true; } 
          else if(chip_id == 0xB5) { modelName = "CC2531"; hasMac = true; }

        String macStr = "N/A (CC111x)";
        if(hasMac && !isLocked) {
            uint8_t mac[8]; cc.read_xdata_memory(0x7FF8, 8, mac);
            macStr = ""; for(int i=0; i<8; i++) { if(mac[i]<0x10) macStr+="0"; macStr+=String(mac[i], HEX); if(i<7) macStr+=":"; }
            macStr.toUpperCase();
        }

        String hexPreview = "";
        if(!isLocked) {
            uint8_t codeBuf[64];
            cc.read_code_memory(0x0000, 64, codeBuf);
            for(int i=0; i<64; i++) {
                if(codeBuf[i] < 0x10) hexPreview += "0";
                hexPreview += String(codeBuf[i], HEX) + " "; 
            }
        } else {
            hexPreview = "LOCKED";
        }
        hexPreview.toUpperCase();

        String json = "{"; 
        json+="\"model\":\""+modelName+"\","; 
        json+="\"rev\":\"0x"+String(chip_rev, HEX)+"\","; 
        json+="\"mac\":\""+macStr+"\","; 
        json+="\"flash\":\""+flashSize+"\","; 
        json+="\"locked\":" + String(isLocked ? "true" : "false") + ","; 
        json+="\"preview\":\""+hexPreview+"\",";
        json+="\"raw\":\""+rawDump+"\""; 
        json+="}";
        
        request->send(200, "application/json", json);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(200, "application/json", getStatusJSON());
    });

    server.on("/api/start_dump", HTTP_GET, [](AsyncWebServerRequest *r){
        if(startDumpTask()) r->send(200, "text/plain", "Dump Start"); 
        else r->send(200, "text/plain", "BUSY"); 
    });
    
    server.on("/api/start_flash", HTTP_GET, [](AsyncWebServerRequest *r){
        if(startFlashTask()) r->send(200, "text/plain", "Flash Start"); 
        else r->send(200, "text/plain", "BUSY");
    });
    
    server.on("/api/start_verify", HTTP_GET, [](AsyncWebServerRequest *r){
        if(startVerifyTask()) r->send(200, "text/plain", "Verify Start"); 
        else r->send(200, "text/plain", "BUSY");
    });

    server.on("/api/lock_chip", HTTP_GET, [](AsyncWebServerRequest *r){
        if(isSystemBusy()) { r->send(200, "text/plain", "BUSY"); return; }
        actionLockChip(NULL);
        r->send(200, "text/plain", "LOCKED");
    });

    server.on("/api/erase_chip", HTTP_GET, [](AsyncWebServerRequest *r){
        if(isSystemBusy()) { r->send(200, "text/plain", "BUSY"); return; }
        if(actionEraseChip()) r->send(200, "text/plain", "ERASED");
        else r->send(500, "text/plain", "FAIL");
    });

    server.on("/download/dump.bin", HTTP_GET, [](AsyncWebServerRequest *r){
        if(LittleFS.exists("/dump.bin")) r->send(LittleFS, "/dump.bin", "application/octet-stream", true); else r->send(404, "text/plain", "No Dump");
    });
    
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *r){ r->send(200); }, [](AsyncWebServerRequest *r, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index){ if(LittleFS.exists("/firmware.bin")) LittleFS.remove("/firmware.bin"); r->_tempFile = LittleFS.open("/firmware.bin", "w"); }
        if(r->_tempFile) r->_tempFile.write(data, len); if(final && r->_tempFile) r->_tempFile.close();
    });

    // --- DEBUGGER APIs ---

    // Query Status (Running vs Halted)
    server.on("/api/debug/status", HTTP_GET, [](AsyncWebServerRequest *r){
        uint8_t s = cc.get_status_byte();
        bool halted = (s & 0x20); // Bit 5
        String json = "{\"halted\":" + String(halted?"true":"false") + ", \"raw\":\"0x" + String(s, HEX) + "\"}";
        r->send(200, "application/json", json);
    });

    server.on("/api/debug/halt", HTTP_GET, [](AsyncWebServerRequest *r){
        cc.debug_halt();
        r->send(200, "text/plain", "HALTED");
    });

    server.on("/api/debug/resume", HTTP_GET, [](AsyncWebServerRequest *r){
        cc.debug_resume();
        r->send(200, "text/plain", "RUNNING");
    });

    server.on("/api/debug/step", HTTP_GET, [](AsyncWebServerRequest *r){
        cc.debug_step();
        r->send(200, "text/plain", "STEPPED");
    });

    // Read RAM/SFR: /api/debug/read?addr=0xF000
    server.on("/api/debug/read", HTTP_GET, [](AsyncWebServerRequest *r){
        if(r->hasParam("addr")) {
            String addrStr = r->getParam("addr")->value();
            uint16_t addr = strtol(addrStr.c_str(), NULL, 16);
            
            uint8_t val = 0;
            cc.read_xdata_memory(addr, 1, &val); // Read 1 Byte
            
            String valHex = String(val, HEX);
            valHex.toUpperCase();
            if(valHex.length()<2) valHex = "0" + valHex;
            
            r->send(200, "application/json", "{\"addr\":\""+addrStr+"\",\"val\":\"0x"+valHex+"\"}");
        } else {
            r->send(400, "text/plain", "Missing addr");
        }
    });

    // Write RAM/SFR: /api/debug/write?addr=0xF000&val=0xFF
    server.on("/api/debug/write", HTTP_GET, [](AsyncWebServerRequest *r){
        if(r->hasParam("addr") && r->hasParam("val")) {
            uint16_t addr = strtol(r->getParam("addr")->value().c_str(), NULL, 16);
            uint8_t val = strtol(r->getParam("val")->value().c_str(), NULL, 16);
            
            uint8_t buf[1] = { val };
            cc.write_xdata_memory(addr, 1, buf);
            
            r->send(200, "text/plain", "OK");
        } else {
            r->send(400, "text/plain", "Missing params");
        }
    });

    // DEBUG: Get all registers
    server.on("/api/debug/registers", HTTP_GET, [](AsyncWebServerRequest *r){
        // Get PC and Registers
        uint16_t pc = cc.read_pc();
        uint8_t r_regs[8];
        cc.read_r0_r7(r_regs);
        
        String json = "{";
        json += "\"PC\":\"0x" + String(pc, HEX) + "\","; 
        json += "\"ACC\":\"0x" + String(cc.read_sfr(0xE0), HEX) + "\",";
        json += "\"B\":\"0x" +   String(cc.read_sfr(0xF0), HEX) + "\",";
        json += "\"PSW\":\"0x" + String(cc.read_sfr(0xD0), HEX) + "\",";
        json += "\"SP\":\"0x" +  String(cc.read_sfr(0x81), HEX) + "\",";
        json += "\"DPL\":\"0x" + String(cc.read_sfr(0x82), HEX) + "\","; 
        json += "\"DPH\":\"0x" + String(cc.read_sfr(0x83), HEX) + "\",";
        json += "\"DPTR\":\"0x" + String(cc.read_sfr(0x83), HEX) + String(cc.read_sfr(0x82), HEX) + "\",";
        json += "\"P0\":\"0x" +  String(cc.read_sfr(0x80), HEX) + "\",";
        json += "\"P1\":\"0x" +  String(cc.read_sfr(0x90), HEX) + "\",";
        json += "\"P2\":\"0x" +  String(cc.read_sfr(0xA0), HEX) + "\",";

        // R-Register Array
        json += "\"R\":[";
        for(int i=0; i<8; i++) {
            json += "\"0x" + String(r_regs[i], HEX) + "\"";
            if(i<7) json += ",";
        }
        json += "]";
        json += "}";
        
        r->send(200, "application/json", json);
    });
    
    // DEBUG: Read Memory Block (for Hex Editor)
    // /api/debug/mem?addr=0xF000&len=64
    server.on("/api/debug/mem", HTTP_GET, [](AsyncWebServerRequest *r){
        uint16_t addr = 0;
        uint16_t len = 16;
        if(r->hasParam("addr")) addr = strtol(r->getParam("addr")->value().c_str(), NULL, 16);
        if(r->hasParam("len")) len = r->getParam("len")->value().toInt();
        
        if(len > 512) len = 512; // Limit
        
        uint8_t* buf = (uint8_t*)malloc(len);
        cc.read_xdata_memory(addr, len, buf);
        
        String hex = "";
        for(int i=0; i<len; i++) {
            if(buf[i]<0x10) hex += "0";
            hex += String(buf[i], HEX);
        }
        free(buf);
        r->send(200, "text/plain", hex);
    });
    
    // SET BREAKPOINT: /api/debug/bp?addr=F123
    // DISABLE: /api/debug/bp?addr=off
    server.on("/api/debug/bp", HTTP_GET, [](AsyncWebServerRequest *r){
        if(r->hasParam("addr")) {
            String val = r->getParam("addr")->value();
            
            // Halt first to be safe
            cc.debug_halt(); 
            
            if(val == "off" || val == "OFF") {
                cc.disable_hw_breakpoint();
                r->send(200, "text/plain", "BP DISABLED");
            } else {
                uint16_t addr = strtol(val.c_str(), NULL, 16);
                cc.set_hw_breakpoint(addr);
                r->send(200, "text/plain", "BP SET @ " + val);
            }
        } else {
            r->send(400, "text/plain", "Missing addr");
        }
    });
    server.begin();
}

void loop() { delay(100); }