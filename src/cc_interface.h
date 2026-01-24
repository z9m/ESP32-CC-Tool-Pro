#pragma once
#include <Arduino.h>

typedef void (*callbackPtr)(uint8_t percent);

class CC_interface
{
  public:
    /**
     * Initialize the CC Interface
     * @param CC Clock Pin
     * @param DD Data Pin
     * @param RESET Reset Pin
     * @return Device ID (16-bit)
     */
    uint16_t begin(uint8_t CC, uint8_t DD, uint8_t RESET);
    
    // Set a callback function for progress updates (0-100%)
    void set_callback(callbackPtr callBack = nullptr);
    
    // Set the Lock Byte (Read Protection)
    uint8_t set_lock_byte(uint8_t lock_byte);
    
    // Perform a full Chip Erase (Unlocks the chip)
    uint8_t erase_chip();
    
    // --- Memory Access ---
    void read_code_memory(uint16_t address, uint16_t len, uint8_t buffer[]);
    void read_xdata_memory(uint16_t address, uint16_t len, uint8_t buffer[]);
    void write_xdata_memory(uint16_t address, uint16_t len, uint8_t buffer[]);
    
    // --- Core Functions ---
    void set_pc(uint16_t address);
    uint8_t clock_init(); // Initialize Debug Clock
    
    // Write firmware to Flash (Code Memory)
    uint8_t write_code_memory(uint16_t address, uint8_t buffer[], int len);
    
    // Verify firmware against buffer
    uint8_t verify_code_memory(uint16_t address, uint8_t buffer[], int len);
    
    // --- Low Level Operations ---
    uint8_t opcode(uint8_t opCode);
    uint8_t opcode(uint8_t opCode, uint8_t opCode1);
    uint8_t opcode(uint8_t opCode, uint8_t opCode1, uint8_t opCode2);
    uint8_t WR_CONFIG(uint8_t config);
    uint8_t WD_CONFIG();
    
    // --- Info & Detection ---
    uint8_t read_chip_info_byte(uint16_t offset);
    uint32_t detect_flash_size();
    
    // --- DEBUGGER FUNCTIONS ---
    void debug_halt();             // Halt the CPU
    void debug_resume();           // Resume execution
    void debug_step();             // Execute single instruction
    uint8_t get_status_byte();     // Read Debug Status Register
    uint8_t read_sfr(uint8_t sfr_addr); // Read Special Function Register
    uint16_t read_pc();            // Read Program Counter
    void read_r0_r7(uint8_t* buffer);   // Read current Register Bank (R0-R7)
    
    // Hardware Breakpoints
    void set_hw_breakpoint(uint16_t address);
    void disable_hw_breakpoint();
    // -------------------------

    /* Send one byte and return one byte as answer */
    uint8_t send_cc_cmdS(uint8_t cmd);
    /* Send one byte and returns two bytes as answer */
    uint16_t send_cc_cmd(uint8_t cmd);
    
    void cc_send_byte(uint8_t in_byte);
    uint8_t cc_receive_byte();
    void enable_cc_debug();
    void reset_cc();

  private:
    boolean dd_direction = 0; // 0=OUT 1=IN
    uint8_t _CC_PIN = -1;
    uint8_t _DD_PIN = -1;
    uint8_t _RESET_PIN = -1;
    
    // Flash Loader Code (8051 Assembly machine code injected into RAM)
    // This small program moves data from RAM (0xF000) to Flash Controller.
    uint8_t flash_opcode[30] = {
      0x75, 0xAD, 0x00, // MOV  FADDRH, #00  ; Set Flash Address High
      0x75, 0xAC, 0x00, // MOV  FADDRL, #00  ; Set Flash Address Low
      0x90, 0xF0, 0x00, // MOV  DPTR, #F000  ; Set Source Pointer (RAM)
      0x75, 0xAE, 0x02, // MOV  FWT,    #02  ; Enable Flash Write
      0x7D, 0x08,       // MOV  R5,     #08  ; Loop Count (placeholder)
      0xE0,             // MOVX A, @DPTR     ; Load byte from RAM
      0xF5, 0xAF,       // MOV  FWDATA, A    ; Write to Flash Data Register
      0xA3,             // INC  DPTR         ; Increment RAM Pointer
      0xE0,             // MOVX A, @DPTR     ; Load next byte (Flash writes are 16-bit aligned)
      0xF5, 0xAF,       // MOV  FWDATA, A    ; Write second byte to trigger write
      0xA3,             // INC  DPTR         ; Increment RAM Pointer
      0xE5, 0xAE,       // MOV  A, FWT       ; Check Flash Write Timing register
      0x20, 0xE6, 0xFB, // JB   FWT.6, $     ; Wait until write complete (Bit 6)
      0xDD, 0xF1,       // DJNZ R5, Loop     ; Decrease count and loop
      0xA5              // DB   0xA5         ; Breakpoint / Done
    };
    callbackPtr _callback = nullptr;
};

extern CC_interface cc;