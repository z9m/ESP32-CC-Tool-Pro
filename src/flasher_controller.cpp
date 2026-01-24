#include "flasher_controller.h"
#include "cc_interface.h"
#include <LittleFS.h>
#include <freertos/semphr.h>

// --- CONFIGURATION ---
const uint32_t CHUNK_SIZE = 1024;

// --- GLOBALS (Internal) ---
static SemaphoreHandle_t statusMutex;
static volatile bool isFlashing = false;
static volatile int globalPercent = 0;
static String globalStatusMsg = "System ready.";

// --- HELPER CLASSES & FUNCTIONS ---

class FileGuard {
    File &_f;
public:
    FileGuard(File &f) : _f(f) {}
    ~FileGuard() { if(_f) _f.close(); }
};

String addrStr(uint32_t addr) {
    String s = String(addr, HEX); s.toUpperCase();
    while(s.length() < 4) s = "0" + s;
    return "0x" + s;
}

void updateStatus(String msg, int pct = -1) {
    if(xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        globalStatusMsg = msg;
        if(pct >= 0) globalPercent = pct;
        xSemaphoreGive(statusMutex);
    }
}

// NEW: Detailed Error Report
// expected = What is in the file (Should be)
// actual   = What was read from chip (Is)
void reportMismatch(uint32_t baseAddr, uint8_t* expected, uint8_t* actual, int len) {
    for(int i=0; i<len; i++) {
        if(expected[i] != actual[i]) {
            String sExp = String(expected[i], HEX); sExp.toUpperCase();
            if(sExp.length() < 2) sExp = "0" + sExp;
            
            String sAct = String(actual[i], HEX); sAct.toUpperCase();
            if(sAct.length() < 2) sAct = "0" + sAct;
            
            // Format: "Mismatch @ 0x1234 (Exp: AA, Act: FF)"
            updateStatus("Error: Mismatch @ " + addrStr(baseAddr+i) + 
                         " (Exp:" + sExp + " Act:" + sAct + ")");
            return;
        }
    }
    // Fallback if memcmp failed but we found no byte diff (rare)
    updateStatus("Error: Verify Fail @ " + addrStr(baseAddr));
}

// --- TASKS IMPLEMENTATION ---

void task_Dump(void * parameter) {
    isFlashing = true; 
    
    updateStatus("BUSY: Init Debug-Mode...", 0);
    cc.enable_cc_debug();
    if(cc.clock_init() != 0) {
        updateStatus("Error: Chip not responding");
        isFlashing = false; vTaskDelete(NULL); return;
    }

    updateStatus("BUSY: Detecting Chip...", 0);
    uint32_t size = cc.detect_flash_size();
    
    if(LittleFS.exists("/dump.bin")) LittleFS.remove("/dump.bin");
    File dumpFile = LittleFS.open("/dump.bin", "w");
    FileGuard fileGuard(dumpFile);

    if(!dumpFile) {
        updateStatus("Error: FS Write Fail");
        isFlashing = false; vTaskDelete(NULL); return;
    }

    // Phase 1: Reading
    updateStatus("BUSY: [1/2] Reading Flash...", 0);
    vTaskDelay(500); 

    uint8_t buffer[CHUNK_SIZE];
    uint32_t addr = 0;
    while(addr < size) {
        uint32_t remaining = size - addr;
        uint16_t len = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        cc.read_code_memory((uint16_t)addr, len, buffer);
        dumpFile.write(buffer, len);
        addr += len;
        if(addr % 2048 == 0) updateStatus("BUSY: [1/2] Reading @ " + addrStr(addr), (addr * 50) / size);
        vTaskDelay(1); 
    }
    dumpFile.close(); 

    // Phase 2: Verify
    updateStatus("BUSY: [2/2] Verifying...", 50);
    vTaskDelay(500); 

    dumpFile = LittleFS.open("/dump.bin", "r");
    FileGuard readGuard(dumpFile);

    addr = 0;
    uint8_t fileBuf[CHUNK_SIZE];
    bool mismatch = false;

    while(addr < size && dumpFile.available()) {
        uint32_t remaining = size - addr;
        uint16_t len = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        
        // Read Chip (Actual)
        cc.read_code_memory((uint16_t)addr, len, buffer);
        // Read File (Expected)
        dumpFile.read(fileBuf, len);
        
        if(memcmp(buffer, fileBuf, len) != 0) {
            mismatch = true;
            reportMismatch(addr, fileBuf, buffer, len);
            break;
        }
        addr += len;
        if(addr % 2048 == 0) updateStatus("BUSY: [2/2] Verifying @ " + addrStr(addr), 50 + ((addr * 50) / size));
        vTaskDelay(1);
    }

    if(!mismatch) updateStatus("DUMP_READY", 100);
    isFlashing = false;
    vTaskDelete(NULL);
}

void task_Flash(void * parameter) {
    isFlashing = true; 
    
    updateStatus("BUSY: Init Debug-Mode...", 0);
    cc.enable_cc_debug();
    cc.clock_init(); 

    updateStatus("BUSY: Preparing...", 0);
    if(!LittleFS.exists("/firmware.bin")){ 
        updateStatus("Error: File missing!"); isFlashing = false; vTaskDelete(NULL); return; 
    }
    
    File fw = LittleFS.open("/firmware.bin", "r");
    FileGuard fwGuard(fw);

    size_t fileSize = fw.size();
    updateStatus("BUSY: Erasing Chip...");
    if(cc.erase_chip() != 0) { 
        fw.close(); 
        LittleFS.remove("/firmware.bin"); 
        updateStatus("Error: Erase Fail!"); isFlashing = false; vTaskDelete(NULL); return; 
    }

    // Phase 1: Writing
    updateStatus("BUSY: [1/2] Writing...", 0);
    vTaskDelay(500);
    uint8_t buffer[CHUNK_SIZE]; 
    uint16_t addr = 0; 
    bool error = false;
    
    while(fw.available()){
        int len = fw.read(buffer, CHUNK_SIZE);
        if(len > 0){
            if(cc.write_code_memory(addr, buffer, len) != 0) { 
                error = true; updateStatus("Error: Write Fail @ " + addrStr(addr)); break; 
            }
            addr += len;
            if(addr % 2048 == 0) updateStatus("BUSY: [1/2] Writing @ " + addrStr(addr), (addr * 50) / fileSize);
            vTaskDelay(1); 
        }
    }
    
    if(error) { LittleFS.remove("/firmware.bin"); isFlashing = false; vTaskDelete(NULL); return; }

    // Phase 2: Verify
    updateStatus("BUSY: [2/2] Verifying...", 50);
    vTaskDelay(500);
    fw.seek(0);
    addr = 0; 
    uint8_t chipBuf[CHUNK_SIZE];
    
    while(fw.available()){
        int len = fw.read(buffer, CHUNK_SIZE);
        if(len > 0){
            // Read Chip
            cc.read_code_memory(addr, len, chipBuf);
            
            if(memcmp(buffer, chipBuf, len) != 0) { 
                error = true; 
                reportMismatch(addr, buffer, chipBuf, len);
                break; 
            }
            
            addr += len;
            if(addr % 2048 == 0) updateStatus("BUSY: [2/2] Checking @ " + addrStr(addr), 50 + ((addr * 50) / fileSize));
            vTaskDelay(1);
        }
    }

    fw.close(); 
    LittleFS.remove("/firmware.bin");
    if(!error) { 
        cc.reset_cc(); 
        updateStatus("Success: Flash & Verify OK!", 100); 
    }
    isFlashing = false; 
    vTaskDelete(NULL);
}

void task_Verify(void * parameter) {
    isFlashing = true; 
    
    updateStatus("BUSY: Init Debug-Mode...", 0);
    cc.enable_cc_debug();
    if(cc.clock_init() != 0) {
        updateStatus("Error: Chip not responding");
        isFlashing = false; vTaskDelete(NULL); return;
    }

    updateStatus("BUSY: Start Verify...", 0);
    if(!LittleFS.exists("/firmware.bin")){ 
        updateStatus("Error: File missing!"); isFlashing = false; vTaskDelete(NULL); return; 
    }
    
    File fw = LittleFS.open("/firmware.bin", "r");
    FileGuard fwGuard(fw);

    size_t fileSize = fw.size();
    uint8_t fileBuf[CHUNK_SIZE]; 
    uint8_t chipBuf[CHUNK_SIZE]; 
    uint16_t addr = 0; 
    bool mismatch = false;

    while(fw.available()){
        int len = fw.read(fileBuf, CHUNK_SIZE);
        if(len > 0){
            // Read Chip
            cc.read_code_memory(addr, len, chipBuf);
            
            if(memcmp(fileBuf, chipBuf, len) != 0) { 
                mismatch = true; 
                reportMismatch(addr, fileBuf, chipBuf, len);
                break; 
            }
            addr += len;
            if(addr % 2048 == 0) updateStatus("BUSY: Checking @ " + addrStr(addr), (addr * 100) / fileSize);
            vTaskDelay(1);
        }
    }
    
    if(!mismatch) updateStatus("Success: Chip identical!", 100);
    isFlashing = false; 
    vTaskDelete(NULL);
}

// --- PUBLIC INTERFACE ---

void initFlasherController() {
    statusMutex = xSemaphoreCreateMutex();
}

String getStatusJSON() {
    String msgCopy;
    int pctCopy;
    if(xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        msgCopy = globalStatusMsg;
        pctCopy = globalPercent;
        xSemaphoreGive(statusMutex);
    } else {
        msgCopy = "Busy/Timeout"; pctCopy = 0;
    }
    return "{\"msg\":\"" + msgCopy + "\",\"pct\":" + String(pctCopy) + "}";
}

bool isSystemBusy() {
    return isFlashing;
}

bool startDumpTask() {
    if(isFlashing) return false;
    xTaskCreate(task_Dump, "Dump", 8192, NULL, 1, NULL);
    return true;
}

bool startFlashTask() {
    if(isFlashing) return false;
    xTaskCreate(task_Flash, "Flash", 8192, NULL, 1, NULL);
    return true;
}

bool startVerifyTask() {
    if(isFlashing) return false;
    xTaskCreate(task_Verify, "Verify", 8192, NULL, 1, NULL);
    return true;
}

void actionLockChip(void (*onSuccess)()) {
    if(isFlashing) return;
    isFlashing = true; 
    updateStatus("BUSY: Setting Lock-Bits...", 0);
    
    cc.enable_cc_debug();
    cc.clock_init();
    cc.set_lock_byte(0x00);
    cc.reset_cc();
    
    if(onSuccess) onSuccess(); 
    
    isFlashing = false;
    updateStatus("Success: Chip Locked (Read Protected)!", 100);
}

bool actionEraseChip() {
    if(isFlashing) return false;
    isFlashing = true; 
    updateStatus("BUSY: Starting Chip Erase...", 0);

    cc.enable_cc_debug();
    cc.clock_init(); 
    uint8_t result = cc.erase_chip();

    if(result == 0) {
        cc.reset_cc();
        updateStatus("Success: Chip erased & unlocked!", 100);
    } else {
        updateStatus("Error: Erase failed (Timeout)!", 0);
    }
    isFlashing = false;
    return (result == 0);
}