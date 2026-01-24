#include <Arduino.h>
#include "cc_interface.h"

CC_interface cc; // Create global instance

uint16_t CC_interface::begin(uint8_t CC, uint8_t DD, uint8_t RESET)
{
  _CC_PIN = CC;
  _DD_PIN = DD;
  _RESET_PIN = RESET;

  pinMode(_CC_PIN, OUTPUT);
  pinMode(_DD_PIN, OUTPUT);
  pinMode(_RESET_PIN, OUTPUT);
  digitalWrite(_CC_PIN, LOW);
  digitalWrite(_DD_PIN, HIGH);
  digitalWrite(_RESET_PIN, HIGH);

  enable_cc_debug();
  uint16_t device_id_answer = send_cc_cmd(0x68); // GET_CHIP_ID
  opcode(0x00); // NOP
  clock_init(); // Try to init clock, return ID even if locked
  return device_id_answer;
}

void CC_interface::set_callback(callbackPtr callBack)
{
  _callback = callBack;
}

uint8_t CC_interface::set_lock_byte(uint8_t lock_byte)
{
  lock_byte = lock_byte & 0x1f; // Mask to max lock byte value
  WR_CONFIG(0x01);              // Select Flash Info Page
  opcode(0x00);                 // NOP

  // Routine to write Lock Bits (See CC253x Flash Controller Reference)
  opcode(0xE5, 0x92);
  opcode(0x75, 0x92, 0x00);
  opcode(0xE5, 0x83);
  opcode(0xE5, 0x82);
  opcode(0x90, 0xF0, 0x00);
  opcode(0x74, 0xFF);
  opcode(0xF0);
  opcode(0xA3);            // Increase Pointer
  opcode(0x74, lock_byte); // Transmit the set lock byte
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x90, 0x00, 0x00);
  opcode(0x75, 0x92, 0x00);
  opcode(0x74, 0x00);

  opcode(0x00); // NOP

  // Configure Timing and Page Erase
  opcode(0xE5, 0x92);
  opcode(0x75, 0x92, 0x00);
  opcode(0xE5, 0x83);
  opcode(0xE5, 0x82);
  opcode(0x90, 0xF8, 0x00);
  opcode(0x74, 0xF0);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x00);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0xDF);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0xAF);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x00);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x02);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x12);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x4A);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x90, 0x00, 0x00);
  opcode(0x75, 0x92, 0x00);
  opcode(0x74, 0x00);

  opcode(0x00); // NOP

  // Execute Write
  opcode(0xE5, 0xC6);
  opcode(0x74, 0x00);
  opcode(0x75, 0xAB, 0x23);
  opcode(0x75, 0xD5, 0xF8);
  opcode(0x75, 0xD4, 0x00);
  opcode(0x75, 0xD6, 0x01);
  opcode(0x75, 0xAD, 0x00);
  opcode(0x75, 0xAC, 0x00);
  opcode(0x75, 0xAE, 0x02);

  opcode(0x00); // NOP

  opcode(0xE5, 0xAE);
  opcode(0x74, 0x00);

  return WR_CONFIG(0x00); // Select normal flash page
}

uint8_t CC_interface::erase_chip()
{
  opcode(0x00); // NOP
  send_cc_cmdS(0x14); // CMD_CHIP_ERASE
  
  // Safe timeout mechanism (unsigned long)
  unsigned long start = millis();
  while (!(send_cc_cmdS(0x34) & 0x80)) // Wait for status bit 7 (Erase Done)
  {
    if (millis() - start > 1000) // Timeout increased to 1s
    {
      return 1; // Timeout Error
    }
  }
  return 0; // Success
}

void CC_interface::read_code_memory(uint32_t address, uint16_t len, uint8_t buffer[])
{
  // 1. Bank berechnen (32KB Blöcke)
  // Bank 0: 0x00000 - 0x07FFF (Physikalisch)
  // Bank 1: 0x08000 - 0x0FFFF (Physikalisch) -> Mapped auf 0x8000
  // Bank 2: 0x10000 - 0x17FFF (Physikalisch) -> Mapped auf 0x8000
  // ...
  uint8_t bank = address / 32768;      // Ganzzahl-Division durch 32k
  uint16_t offset = address % 32768;   // Rest ist der Offset im 32k Block
  uint16_t virtual_addr;

  if (bank == 0) {
    // Bank 0 liegt beim 8051 immer fest im unteren Bereich (0x0000 - 0x7FFF)
    virtual_addr = offset;
    // Wir müssen MEMCTR hier nicht zwingend setzen, da Bank 0 immer sichtbar ist,
    // aber wir setzen MEMCTR auf 1, damit der obere Bereich definiert ist (Best Practice)
    opcode(0x75, 0xC7, 0x01); 
  } else {
    // Alle anderen Banks (1-7) werden in das obere Fenster (0x8000 - 0xFFFF) eingeblendet
    virtual_addr = 0x8000 + offset;
    
    // Bank auswählen: MOV MEMCTR, #bank
    // Opcode 0x75 = MOV direct, #data. 0xC7 = Adresse von MEMCTR.
    opcode(0x75, 0xC7, bank); 
  }

  // 2. DPTR setzen (Data Pointer)
  opcode(0x90, virtual_addr >> 8, virtual_addr);

  // 3. Lesen (Schleife wie vorher)
  int last_callback = 0;
  for (int i = 0; i < len; i++)
  {
    opcode(0xe4); // CLR A
    buffer[i] = opcode(0x93); // MOVC A, @A+DPTR (Liest Byte aus Flash)
    opcode(0xa3); // INC DPTR
    
    // Fortschrittsanzeige Update
    if (i - last_callback > 100)
    {
      last_callback = i;
      if (_callback != nullptr)
      {
        uint8_t percent = ((float)((float)i / (float)len) * 100.0);
        if (percent > 100) percent = 100;
        _callback(percent);
      }
    }
  }
  if (_callback != nullptr)
    _callback(100);
}

void CC_interface::read_xdata_memory(uint16_t address, uint16_t len, uint8_t buffer[])
{
  opcode(0x90, address >> 8, address); // MOV DPTR
  for (int i = 0; i < len; i++)
  {
    buffer[i] = opcode(0xe0); // MOVX A, @DPTR
    opcode(0xa3); // INC DPTR
  }
}

void CC_interface::write_xdata_memory(uint16_t address, uint16_t len, uint8_t buffer[])
{
  opcode(0x90, address >> 8, address); // MOV DPTR
  for (int i = 0; i < len; i++)
  {
    opcode(0x74, buffer[i]); // MOV A, #data
    opcode(0xf0); // MOVX @DPTR, A
    opcode(0xa3); // INC DPTR
  }
}

void CC_interface::set_pc(uint16_t address)
{
  opcode(0x02, address >> 8, address); // LJMP
}

uint8_t CC_interface::clock_init()
{
  opcode(0x75, 0xc6, 0x00); // CLKCON
  
  // Safe Timeout
  unsigned long start = millis();
  while (!(opcode(0xe5, 0xbe) & 0x40))
  {
    if (millis() - start > 200)
    {
      return 1;
    }
  }
  return 0;
}

uint8_t CC_interface::write_code_memory(uint16_t address, uint8_t buffer[], int len)
{
  int entry_len = len;
  if (len % 2 != 0)
    len++;
  int position = 0;
  int len_per_transfer = 64;
  address = address / 2; // Word addressing for Flash
  
  while (len)
  {
    // Prepare Flash-Loader Code in RAM
    flash_opcode[2] = (address >> 8) & 0xff;
    flash_opcode[5] = address & 0xff;
    flash_opcode[13] = (len > len_per_transfer) ? (len_per_transfer / 2) : (len / 2);
    
    write_xdata_memory(0xf000, len_per_transfer, &buffer[position]);
    write_xdata_memory(0xf100, sizeof(flash_opcode), flash_opcode);
    
    opcode(0x75, 0xC7, 0x51); // MEMCTR
    set_pc(0xf100);
    send_cc_cmdS(0x4c); // Resume Execution
    
    // Safe Timeout for Execution
    unsigned long start = millis();
    while (!(send_cc_cmdS(0x34) & 0x08)) // Wait for CPU Idle (0x08 in Status byte)
    {
      if (millis() - start > 500)
      {
        if (_callback != nullptr) _callback(0);
        return 1; // Timeout during write
      }
    }
    
    if (_callback != nullptr)
    {
      uint8_t percent = 100 - ((float)((float)len / (float)entry_len) * 100.0);
      if (percent > 100) percent = 100;
      _callback(percent);
    }
    len -= flash_opcode[13] * 2;
    position += flash_opcode[13] * 2;
    address += flash_opcode[13];
  }
  
  if (_callback != nullptr)
    _callback(100);
  return 0;
}

uint8_t CC_interface::verify_code_memory(uint16_t address, uint8_t buffer[], int len)
{
  int last_callback = 0;
  opcode(0x75, 0xc7, 0x01);
  opcode(0x90, address >> 8, address);
  for (int i = 0; i < len; i++)
  {
    opcode(0xe4);
    if (buffer[i] != opcode(0x93))
    {
      if (_callback != nullptr) _callback(0);
      return 1; // Mismatch
    }
    opcode(0xa3);
    if (i - last_callback > 100)
    {
      last_callback = i;
      if (_callback != nullptr)
      {
        uint8_t percent = ((float)((float)i / (float)len) * 100.0);
        if (percent > 100) percent = 100;
        _callback(percent);
      }
    }
  }
  if (_callback != nullptr)
    _callback(100);
  return 0;
}

uint8_t CC_interface::opcode(uint8_t opCode)
{
  cc_send_byte(0x55);
  cc_send_byte(opCode);
  return cc_receive_byte();
}

uint8_t CC_interface::opcode(uint8_t opCode, uint8_t opCode1)
{
  cc_send_byte(0x56);
  cc_send_byte(opCode);
  cc_send_byte(opCode1);
  return cc_receive_byte();
}

uint8_t CC_interface::opcode(uint8_t opCode, uint8_t opCode1, uint8_t opCode2)
{
  cc_send_byte(0x57);
  cc_send_byte(opCode);
  cc_send_byte(opCode1);
  cc_send_byte(opCode2);
  return cc_receive_byte();
}

uint8_t CC_interface::WR_CONFIG(uint8_t config)
{
  cc_send_byte(0x1d);
  cc_send_byte(config);
  return cc_receive_byte();
}

uint8_t CC_interface::WD_CONFIG()
{
  cc_send_byte(0x24);
  return cc_receive_byte();
}

uint8_t CC_interface::send_cc_cmdS(uint8_t cmd)
{
  cc_send_byte(cmd);
  return cc_receive_byte();
}

uint16_t CC_interface::send_cc_cmd(uint8_t cmd)
{
  cc_send_byte(cmd);
  return (cc_receive_byte() << 8) + cc_receive_byte();
}

// Atomic Bit-Banging (Disable Interrupts)
void CC_interface::cc_send_byte(uint8_t in_byte)
{
  if (dd_direction == 1)
  {
    dd_direction = 0;
    pinMode(_DD_PIN, OUTPUT);
    digitalWrite(_DD_PIN, LOW);
  }
  
  // CRITICAL SECTION START
  // Prevents WiFi interrupts from breaking 8-Bit timing
  noInterrupts(); 
  
  for (int i = 8; i; i--)
  {
    if (in_byte & 0x80)
      digitalWrite(_DD_PIN, HIGH);
    else
      digitalWrite(_DD_PIN, LOW);

    digitalWrite(_CC_PIN, HIGH);
    in_byte <<= 1;
    delayMicroseconds(5);
    digitalWrite(_CC_PIN, LOW);
  }
  
  interrupts(); 
  // CRITICAL SECTION END
}

// Atomic Bit-Banging (Disable Interrupts)
uint8_t CC_interface::cc_receive_byte()
{
  uint8_t out_byte = 0x00;
  if (dd_direction == 0)
  {
    dd_direction = 1;
    pinMode(_DD_PIN, INPUT);
    digitalWrite(_DD_PIN, LOW);
  }
  
  // CRITICAL SECTION START
  noInterrupts();
  
  for (int i = 8; i; i--)
  {
    digitalWrite(_CC_PIN, HIGH);
    delayMicroseconds(5);
    out_byte <<= 1;
    if (digitalRead(_DD_PIN))
      out_byte |= 0x01;
    digitalWrite(_CC_PIN, LOW);
    delayMicroseconds(5);
  }
  
  interrupts();
  // CRITICAL SECTION END
  
  return out_byte;
}

void CC_interface::enable_cc_debug()
{
  if (dd_direction == 0)
  {
    dd_direction = 1;
    pinMode(_DD_PIN, INPUT);
    digitalWrite(_DD_PIN, HIGH);
  }
  delay(5);
  digitalWrite(_RESET_PIN, LOW);
  delay(2);
  digitalWrite(_CC_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(_CC_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(_CC_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(_CC_PIN, LOW);
  delay(2);
  digitalWrite(_RESET_PIN, HIGH);
  delay(2);
}

void CC_interface::reset_cc()
{
  if (dd_direction == 0)
  {
    dd_direction = 1;
    pinMode(_DD_PIN, INPUT);
    digitalWrite(_DD_PIN, HIGH);
  }
  delay(5);
  digitalWrite(_RESET_PIN, LOW);
  delay(5);
  digitalWrite(_RESET_PIN, HIGH);
  delay(2);
}

uint8_t CC_interface::read_chip_info_byte(uint16_t offset)
{
  WR_CONFIG(0x01); 
  uint8_t val = 0;
  read_xdata_memory(offset, 1, &val);
  WR_CONFIG(0x00);
  return val;
}

// Intelligent Size Detection (with Aliasing Check)
uint32_t CC_interface::detect_flash_size() {
    uint16_t raw_id = send_cc_cmd(0x68);
    uint8_t chip_id = (raw_id >> 8) & 0xFF;
    
    uint32_t max_size = 32768; 
    if (chip_id == 0xA5 || chip_id == 0xB5) { 
        max_size = 262144; 
    }

    uint8_t buf_0[8];
    uint8_t buf_8k[8];
    uint8_t buf_16k[8];
    
    read_xdata_memory(0x0000, 8, buf_0);
    read_xdata_memory(0x2000, 8, buf_8k);  
    read_xdata_memory(0x4000, 8, buf_16k); 
    
    bool isEmpty = true;
    for(int i=0; i<8; i++) {
        if(buf_0[i] != 0xFF) { isEmpty = false; break; }
    }
    
    if(isEmpty) return max_size;

    if (memcmp(buf_0, buf_8k, 8) == 0 && memcmp(buf_0, buf_16k, 8) == 0) {
         return 8192; 
    }

    if (memcmp(buf_0, buf_16k, 8) == 0) {
        return 16384; 
    }

    return max_size; 
}

void CC_interface::debug_halt()
{
  // Halt command (same as enable_cc_debug sequence)
  enable_cc_debug(); // Forces Chip into Halt-Mode
}

void CC_interface::debug_resume()
{
  // 0x4C = CMD_RESUME
  send_cc_cmdS(0x4C);
}

void CC_interface::debug_step()
{
  // 0x5C = CMD_STEP_INSTR (Executes one instruction)
  send_cc_cmdS(0x5C);
}

uint8_t CC_interface::get_status_byte()
{
  // 0x34 = CMD_GET_STATUS
  return send_cc_cmdS(0x34);
}

// Read SFR (Special Function Register)
// Trick: We inject "MOV A, sfrAddr" -> "MOV DPTR, 0xF000" -> "MOVX @DPTR, A"
uint8_t CC_interface::read_sfr(uint8_t sfr_addr) {
    // 1. opcode 0xE5 = MOV A, direct
    opcode(0xE5, sfr_addr); 
    
    // 2. DPTR to 0xF000 (Scratchpad RAM)
    opcode(0x90, 0xF0, 0x00);
    
    // 3. opcode 0xF0 = MOVX @DPTR, A (Write A to RAM)
    opcode(0xF0);
    
    // 4. Read from RAM
    uint8_t val = 0;
    read_xdata_memory(0xF000, 1, &val);
    return val;
}

void CC_interface::set_hw_breakpoint(uint16_t address)
{
  // 1. Write Breakpoint Addr to Debug Registers (XDATA Mapped)
  // CC Chips usually use 0xC760 (Low) and 0xC761 (High) for BP0
  uint8_t bp_low = address & 0xFF;
  uint8_t bp_high = (address >> 8) & 0xFF;
  
  uint8_t data[2] = { bp_low, bp_high };
  write_xdata_memory(0xC760, 2, data);

  // 2. Enable Breakpoint (Control Register 0xC768, Bit 0 = Enable BP0)
  uint8_t ctrl = 0x01; // Enable BP0
  write_xdata_memory(0xC768, 1, &ctrl);
}

void CC_interface::disable_hw_breakpoint()
{
  // Disable BP0 (Bit 0 to 0)
  uint8_t ctrl = 0x00;
  write_xdata_memory(0xC768, 1, &ctrl);
}

// Reads the Program Counter via Stack-Trick (Non-destructive)
uint16_t CC_interface::read_pc() {
    // 1. Save Stack Pointer (SP)
    uint8_t sp_orig = read_sfr(0x81); 

    // 2. Inject LCALL 0xF000. 
    // This pushes the current PC onto the stack and sets PC to 0xF000.
    opcode(0x12, 0xF0, 0x00); 

    // Helper Variable
    uint8_t pc_parts[2] = {0, 0}; // [0]=High, [1]=Low

    // 3. Retrieve original PC from Internal Stack (IDATA)
    // High Byte is at SP+2, Low Byte at SP+1
    for(int i=0; i<2; i++) {
        uint8_t target_idata_addr = sp_orig + 2 - i; 
        
        // A) Move IDATA value to R0
        opcode(0x78, target_idata_addr); // MOV R0, #addr
        
        // B) Move value from IDATA (@R0) to Accumulator (A)
        opcode(0xE6); // MOV A, @R0 
        
        // C) Move A to XDATA 0xF000 (our scratchpad)
        opcode(0x90, 0xF0, 0x00); // MOV DPTR, #0xF000
        opcode(0xF0);             // MOVX @DPTR, A
        
        // D) Read from XDATA
        read_xdata_memory(0xF000, 1, &pc_parts[i]);
    }

    // 4. Restore Stack Pointer
    opcode(0x75, 0x81, sp_orig); // MOV SP, #orig

    // 5. RESTORE PROGRAM COUNTER (The Fix!)
    // We must jump back to where we came from, otherwise PC stays at 0xF000.
    // Opcode 0x02 = LJMP (Long Jump)
    opcode(0x02, pc_parts[0], pc_parts[1]);

    return (uint16_t)(pc_parts[0] << 8) | pc_parts[1];
}

// Reads R0-R7 based on the current Register Bank
void CC_interface::read_r0_r7(uint8_t* buffer) {
    uint8_t psw = read_sfr(0xD0);     // Read PSW
    uint8_t bank = (psw >> 3) & 0x03; // Bits 3 and 4 are Bank Select
    uint16_t addr = bank * 8;         // Bank 0 = 0x00, Bank 1 = 0x08...
    read_xdata_memory(addr, 8, buffer);
}