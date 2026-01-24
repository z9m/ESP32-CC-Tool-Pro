#pragma once
#include <Arduino.h>

const char javaScript[] PROGMEM = R"rawliteral(
  
  // --- GLOBALS ---
  let curLang = 'de';
  let debugTimer = null;
  let currentAction = '';
  let espPins = { clk: '?', data: '?', rst: '?' };
  let translations = {};
  let lastLogMsg = ""; 
  let viewMode = 'asm'; // oder 'hex'


  const targets = {
    'std': { name: 'Standard', map: { data: 'P2.2 (Data)', clk: 'P2.1 (Clock)', rst: 'RESET_N', gnd: 'GND', vcc: 'VDD' } },
    'chroma74': { name: 'Chroma 74', map: { data: 'TP3 (Data)', clk: 'TP5 (Clock)', rst: 'TP2 (Reset)', gnd: 'TP8 (GND)', vcc: 'TP4 (3.3V)' } }
  };

  window.onload = function() {
      let saved = localStorage.getItem('cc_lang');
      if(saved) curLang = saved;
      else {
          let navLang = navigator.language.substring(0,2);
          if(['de','en','es','fr','it','pl','cs','ja','zh'].includes(navLang)) curLang = navLang;
      }
      let langSel = document.getElementById('langSelect');
      if(langSel) langSel.value = curLang;

      fetch('/api/lang').then(r=>r.json()).then(l => {
          translations = l; updateTexts(); 
          fetch('/api/pins').then(r=>r.json()).then(p => { espPins = p; updatePinoutView(); });
          fetch('/api/system_info').then(r=>r.json()).then(info => { log("System IP: " + info.ip); autoConnect(); });
      }).catch(e=>{ log("Init Error: " + e); });
  };

  // --- 8051 DISASSEMBLER ENGINE ---

// Mapping: Opcode -> { len: Länge in Bytes, mask: Format-String }
// $1 = Erstes Argument (Byte nach Opcode)
// $2 = Zweites Argument
// rel = Relativer Sprung (wird berechnet)
// bit = Bit Adresse
const OPCODES = {
  0x00: { l:1, t:"NOP" },
  0x01: { l:2, t:"AJMP $1" },
  0x02: { l:3, t:"LJMP $1$2" },
  0x03: { l:1, t:"RR A" },
  0x04: { l:1, t:"INC A" },
  0x05: { l:2, t:"INC $1" },
  0x06: { l:1, t:"INC @R0" },
  0x07: { l:1, t:"INC @R1" },
  0x08: { l:1, t:"INC R0" },
  0x09: { l:1, t:"INC R1" },
  0x0A: { l:1, t:"INC R2" },
  0x0B: { l:1, t:"INC R3" },
  0x0C: { l:1, t:"INC R4" },
  0x0D: { l:1, t:"INC R5" },
  0x0E: { l:1, t:"INC R6" },
  0x0F: { l:1, t:"INC R7" },
  0x10: { l:3, t:"JBC $1, $rel" },
  0x11: { l:2, t:"ACALL $1" },
  0x12: { l:3, t:"LCALL $1$2" },
  0x13: { l:1, t:"RRC A" },
  0x14: { l:1, t:"DEC A" },
  0x15: { l:2, t:"DEC $1" },
  0x16: { l:1, t:"DEC @R0" },
  0x17: { l:1, t:"DEC @R1" },
  0x18: { l:1, t:"DEC R0" },
  0x19: { l:1, t:"DEC R1" },
  0x1A: { l:1, t:"DEC R2" },
  0x1B: { l:1, t:"DEC R3" },
  0x1C: { l:1, t:"DEC R4" },
  0x1D: { l:1, t:"DEC R5" },
  0x1E: { l:1, t:"DEC R6" },
  0x1F: { l:1, t:"DEC R7" },
  0x20: { l:3, t:"JB $1, $rel" },
  0x21: { l:2, t:"AJMP $1" },
  0x22: { l:1, t:"RET" },
  0x23: { l:1, t:"RL A" },
  0x24: { l:2, t:"ADD A, #$1" },
  0x25: { l:2, t:"ADD A, $1" },
  0x26: { l:1, t:"ADD A, @R0" },
  0x27: { l:1, t:"ADD A, @R1" },
  0x28: { l:1, t:"ADD A, R0" },
  0x29: { l:1, t:"ADD A, R1" },
  0x2A: { l:1, t:"ADD A, R2" },
  0x2B: { l:1, t:"ADD A, R3" },
  0x2C: { l:1, t:"ADD A, R4" },
  0x2D: { l:1, t:"ADD A, R5" },
  0x2E: { l:1, t:"ADD A, R6" },
  0x2F: { l:1, t:"ADD A, R7" },
  0x30: { l:3, t:"JNB $1, $rel" },
  0x31: { l:2, t:"ACALL $1" },
  0x32: { l:1, t:"RETI" },
  0x33: { l:1, t:"RLC A" },
  0x34: { l:2, t:"ADDC A, #$1" },
  0x35: { l:2, t:"ADDC A, $1" },
  0x36: { l:1, t:"ADDC A, @R0" },
  0x37: { l:1, t:"ADDC A, @R1" },
  0x38: { l:1, t:"ADDC A, R0" },
  0x39: { l:1, t:"ADDC A, R1" },
  0x3A: { l:1, t:"ADDC A, R2" },
  0x3B: { l:1, t:"ADDC A, R3" },
  0x3C: { l:1, t:"ADDC A, R4" },
  0x3D: { l:1, t:"ADDC A, R5" },
  0x3E: { l:1, t:"ADDC A, R6" },
  0x3F: { l:1, t:"ADDC A, R7" },
  0x40: { l:2, t:"JC $rel" },
  0x41: { l:2, t:"AJMP $1" },
  0x42: { l:2, t:"ORL $1, A" },
  0x43: { l:3, t:"ORL $1, #$2" },
  0x44: { l:2, t:"ORL A, #$1" },
  0x45: { l:2, t:"ORL A, $1" },
  0x46: { l:1, t:"ORL A, @R0" },
  0x47: { l:1, t:"ORL A, @R1" },
  0x48: { l:1, t:"ORL A, R0" },
  0x49: { l:1, t:"ORL A, R1" },
  0x4A: { l:1, t:"ORL A, R2" },
  0x4B: { l:1, t:"ORL A, R3" },
  0x4C: { l:1, t:"ORL A, R4" },
  0x4D: { l:1, t:"ORL A, R5" },
  0x4E: { l:1, t:"ORL A, R6" },
  0x4F: { l:1, t:"ORL A, R7" },
  0x50: { l:2, t:"JNC $rel" },
  0x51: { l:2, t:"ACALL $1" },
  0x52: { l:2, t:"ANL $1, A" },
  0x53: { l:3, t:"ANL $1, #$2" },
  0x54: { l:2, t:"ANL A, #$1" },
  0x55: { l:2, t:"ANL A, $1" },
  0x56: { l:1, t:"ANL A, @R0" },
  0x57: { l:1, t:"ANL A, @R1" },
  0x58: { l:1, t:"ANL A, R0" },
  0x59: { l:1, t:"ANL A, R1" },
  0x5A: { l:1, t:"ANL A, R2" },
  0x5B: { l:1, t:"ANL A, R3" },
  0x5C: { l:1, t:"ANL A, R4" },
  0x5D: { l:1, t:"ANL A, R5" },
  0x5E: { l:1, t:"ANL A, R6" },
  0x5F: { l:1, t:"ANL A, R7" },
  0x60: { l:2, t:"JZ $rel" },
  0x61: { l:2, t:"AJMP $1" },
  0x62: { l:2, t:"XRL $1, A" },
  0x63: { l:3, t:"XRL $1, #$2" },
  0x64: { l:2, t:"XRL A, #$1" },
  0x65: { l:2, t:"XRL A, $1" },
  0x66: { l:1, t:"XRL A, @R0" },
  0x67: { l:1, t:"XRL A, @R1" },
  0x68: { l:1, t:"XRL A, R0" },
  0x69: { l:1, t:"XRL A, R1" },
  0x6A: { l:1, t:"XRL A, R2" },
  0x6B: { l:1, t:"XRL A, R3" },
  0x6C: { l:1, t:"XRL A, R4" },
  0x6D: { l:1, t:"XRL A, R5" },
  0x6E: { l:1, t:"XRL A, R6" },
  0x6F: { l:1, t:"XRL A, R7" },
  0x70: { l:2, t:"JNZ $rel" },
  0x71: { l:2, t:"ACALL $1" },
  0x72: { l:2, t:"ORL C, $1" },
  0x73: { l:1, t:"JMP @A+DPTR" },
  0x74: { l:2, t:"MOV A, #$1" },
  0x75: { l:3, t:"MOV $1, #$2" },
  0x76: { l:2, t:"MOV @R0, #$1" },
  0x77: { l:2, t:"MOV @R1, #$1" },
  0x78: { l:2, t:"MOV R0, #$1" },
  0x79: { l:2, t:"MOV R1, #$1" },
  0x7A: { l:2, t:"MOV R2, #$1" },
  0x7B: { l:2, t:"MOV R3, #$1" },
  0x7C: { l:2, t:"MOV R4, #$1" },
  0x7D: { l:2, t:"MOV R5, #$1" },
  0x7E: { l:2, t:"MOV R6, #$1" },
  0x7F: { l:2, t:"MOV R7, #$1" },
  0x80: { l:2, t:"SJMP $rel" },
  0x81: { l:2, t:"AJMP $1" },
  0x82: { l:2, t:"ANL C, $1" },
  0x83: { l:1, t:"MOVC A, @A+PC" },
  0x84: { l:1, t:"DIV AB" },
  0x85: { l:3, t:"MOV $2, $1" },
  0x86: { l:2, t:"MOV $1, @R0" },
  0x87: { l:2, t:"MOV $1, @R1" },
  0x88: { l:2, t:"MOV $1, R0" },
  0x89: { l:2, t:"MOV $1, R1" },
  0x8A: { l:2, t:"MOV $1, R2" },
  0x8B: { l:2, t:"MOV $1, R3" },
  0x8C: { l:2, t:"MOV $1, R4" },
  0x8D: { l:2, t:"MOV $1, R5" },
  0x8E: { l:2, t:"MOV $1, R6" },
  0x8F: { l:2, t:"MOV $1, R7" },
  0x90: { l:3, t:"MOV DPTR, #$1$2" },
  0x91: { l:2, t:"ACALL $1" },
  0x92: { l:2, t:"MOV $1, C" },
  0x93: { l:1, t:"MOVC A, @A+DPTR" },
  0x94: { l:2, t:"SUBB A, #$1" },
  0x95: { l:2, t:"SUBB A, $1" },
  0x96: { l:1, t:"SUBB A, @R0" },
  0x97: { l:1, t:"SUBB A, @R1" },
  0x98: { l:1, t:"SUBB A, R0" },
  0x99: { l:1, t:"SUBB A, R1" },
  0x9A: { l:1, t:"SUBB A, R2" },
  0x9B: { l:1, t:"SUBB A, R3" },
  0x9C: { l:1, t:"SUBB A, R4" },
  0x9D: { l:1, t:"SUBB A, R5" },
  0x9E: { l:1, t:"SUBB A, R6" },
  0x9F: { l:1, t:"SUBB A, R7" },
  0xA0: { l:2, t:"ORL C, /$1" },
  0xA1: { l:2, t:"AJMP $1" },
  0xA2: { l:2, t:"MOV C, $1" },
  0xA3: { l:1, t:"INC DPTR" },
  0xA4: { l:1, t:"MUL AB" },
  0xA5: { l:1, t:"DB 0xA5" }, // Reserved
  0xA6: { l:2, t:"MOV @R0, $1" },
  0xA7: { l:2, t:"MOV @R1, $1" },
  0xA8: { l:2, t:"MOV R0, $1" },
  0xA9: { l:2, t:"MOV R1, $1" },
  0xAA: { l:2, t:"MOV R2, $1" },
  0xAB: { l:2, t:"MOV R3, $1" },
  0xAC: { l:2, t:"MOV R4, $1" },
  0xAD: { l:2, t:"MOV R5, $1" },
  0xAE: { l:2, t:"MOV R6, $1" },
  0xAF: { l:2, t:"MOV R7, $1" },
  0xB0: { l:2, t:"ANL C, /$1" },
  0xB1: { l:2, t:"ACALL $1" },
  0xB2: { l:2, t:"CPL $1" },
  0xB3: { l:1, t:"CPL C" },
  0xB4: { l:3, t:"CJNE A, #$1, $rel" },
  0xB5: { l:3, t:"CJNE A, $1, $rel" },
  0xB6: { l:3, t:"CJNE @R0, #$1, $rel" },
  0xB7: { l:3, t:"CJNE @R1, #$1, $rel" },
  0xB8: { l:3, t:"CJNE R0, #$1, $rel" },
  0xB9: { l:3, t:"CJNE R1, #$1, $rel" },
  0xBA: { l:3, t:"CJNE R2, #$1, $rel" },
  0xBB: { l:3, t:"CJNE R3, #$1, $rel" },
  0xBC: { l:3, t:"CJNE R4, #$1, $rel" },
  0xBD: { l:3, t:"CJNE R5, #$1, $rel" },
  0xBE: { l:3, t:"CJNE R6, #$1, $rel" },
  0xBF: { l:3, t:"CJNE R7, #$1, $rel" },
  0xC0: { l:2, t:"PUSH $1" },
  0xC1: { l:2, t:"AJMP $1" },
  0xC2: { l:2, t:"CLR $1" },
  0xC3: { l:1, t:"CLR C" },
  0xC4: { l:1, t:"SWAP A" },
  0xC5: { l:2, t:"XCH A, $1" },
  0xC6: { l:1, t:"XCH A, @R0" },
  0xC7: { l:1, t:"XCH A, @R1" },
  0xC8: { l:1, t:"XCH A, R0" },
  0xC9: { l:1, t:"XCH A, R1" },
  0xCA: { l:1, t:"XCH A, R2" },
  0xCB: { l:1, t:"XCH A, R3" },
  0xCC: { l:1, t:"XCH A, R4" },
  0xCD: { l:1, t:"XCH A, R5" },
  0xCE: { l:1, t:"XCH A, R6" },
  0xCF: { l:1, t:"XCH A, R7" },
  0xD0: { l:2, t:"POP $1" },
  0xD1: { l:2, t:"ACALL $1" },
  0xD2: { l:2, t:"SETB $1" },
  0xD3: { l:1, t:"SETB C" },
  0xD4: { l:1, t:"DA A" },
  0xD5: { l:3, t:"DJNZ $1, $rel" },
  0xD6: { l:1, t:"XCHD A, @R0" },
  0xD7: { l:1, t:"XCHD A, @R1" },
  0xD8: { l:2, t:"DJNZ R0, $rel" },
  0xD9: { l:2, t:"DJNZ R1, $rel" },
  0xDA: { l:2, t:"DJNZ R2, $rel" },
  0xDB: { l:2, t:"DJNZ R3, $rel" },
  0xDC: { l:2, t:"DJNZ R4, $rel" },
  0xDD: { l:2, t:"DJNZ R5, $rel" },
  0xDE: { l:2, t:"DJNZ R6, $rel" },
  0xDF: { l:2, t:"DJNZ R7, $rel" },
  0xE0: { l:1, t:"MOVX A, @DPTR" },
  0xE1: { l:2, t:"AJMP $1" },
  0xE2: { l:1, t:"MOVX A, @R0" },
  0xE3: { l:1, t:"MOVX A, @R1" },
  0xE4: { l:1, t:"CLR A" },
  0xE5: { l:2, t:"MOV A, $1" },
  0xE6: { l:1, t:"MOV A, @R0" },
  0xE7: { l:1, t:"MOV A, @R1" },
  0xE8: { l:1, t:"MOV A, R0" },
  0xE9: { l:1, t:"MOV A, R1" },
  0xEA: { l:1, t:"MOV A, R2" },
  0xEB: { l:1, t:"MOV A, R3" },
  0xEC: { l:1, t:"MOV A, R4" },
  0xED: { l:1, t:"MOV A, R5" },
  0xEE: { l:1, t:"MOV A, R6" },
  0xEF: { l:1, t:"MOV A, R7" },
  0xF0: { l:1, t:"MOVX @DPTR, A" },
  0xF1: { l:2, t:"ACALL $1" },
  0xF2: { l:1, t:"MOVX @R0, A" },
  0xF3: { l:1, t:"MOVX @R1, A" },
  0xF4: { l:1, t:"CPL A" },
  0xF5: { l:2, t:"MOV $1, A" },
  0xF6: { l:1, t:"MOV @R0, A" },
  0xF7: { l:1, t:"MOV @R1, A" },
  0xF8: { l:1, t:"MOV R0, A" },
  0xF9: { l:1, t:"MOV R1, A" },
  0xFA: { l:1, t:"MOV R2, A" },
  0xFB: { l:1, t:"MOV R3, A" },
  0xFC: { l:1, t:"MOV R4, A" },
  0xFD: { l:1, t:"MOV R5, A" },
  0xFE: { l:1, t:"MOV R6, A" },
  0xFF: { l:1, t:"MOV R7, A" }
};

function disassembleBlock(startAddr, bytes) {
    let output = [];
    let i = 0;
    
    while(i < bytes.length) {
        let addr = startAddr + i;
        let op = bytes[i];
        let def = OPCODES[op];
        
        let hexStr = "";
        let mnemonic = "";
        let len = 1;

        if (def) {
            len = def.l;
            // Sicherstellen, dass wir genug Bytes im Buffer haben
            if (i + len > bytes.length) {
                mnemonic = "??? (incomplete)";
                hexStr = bytes.slice(i).map(b=>b.toString(16).padStart(2,'0')).join(" ");
                i = bytes.length; // Abbrechen
            } else {
                // Argumente extrahieren
                let arg1 = (len > 1) ? bytes[i+1] : 0;
                let arg2 = (len > 2) ? bytes[i+2] : 0;
                
                // Hex String für Anzeige bauen
                for(let k=0; k<len; k++) hexStr += bytes[i+k].toString(16).toUpperCase().padStart(2,'0') + " ";
                
                // Mnemonic formatieren
                mnemonic = def.t;
                
                // Ersetzungen
                mnemonic = mnemonic.replace("$1", arg1.toString(16).toUpperCase().padStart(2,'0'));
                mnemonic = mnemonic.replace("$2", arg2.toString(16).toUpperCase().padStart(2,'0'));
                
                // Relative Sprünge berechnen (SJMP, DJNZ etc.)
                if (mnemonic.includes("$rel")) {
                    // Argument ist signed byte offset
                    let offset = arg1;
                    if (offset > 127) offset -= 256;
                    // Ziel = Aktuelle Instruction Addr + Instruction Length + Offset
                    let dest = addr + len + offset; 
                    mnemonic = mnemonic.replace("$rel", "0x" + dest.toString(16).toUpperCase().padStart(4,'0'));
                }
            }
        } else {
            // Unbekannter Opcode -> Einfach als DB (Define Byte) anzeigen
            hexStr = op.toString(16).toUpperCase().padStart(2,'0');
            mnemonic = `DB 0x${hexStr.trim()}`;
        }

        output.push({
            addr: addr,
            addrStr: "0x" + addr.toString(16).toUpperCase().padStart(4,'0'),
            hex: hexStr,
            asm: mnemonic,
            rawLen: len
        });
        
        i += len;
    }
    return output;
  }

  function switchTab(id) {
      document.querySelectorAll('.tab').forEach(el => el.classList.remove('active'));
      document.querySelectorAll('.content-area').forEach(el => el.classList.remove('active'));
      let btn = document.querySelector(`.tab[onclick="switchTab('${id}')"]`);
      if(btn) btn.classList.add('active');
      let content = document.getElementById(id);
      if(content) content.classList.add('active');
      if(id === 'debugger') startDebugPoll(); else stopDebugPoll();
  }

  // --- UI HELPER ---
  function t(key) { return (translations[curLang] && translations[curLang][key]) ? translations[curLang][key] : key; }
  function changeLang(val) { curLang = val; localStorage.setItem('cc_lang', val); updateTexts(); updatePinoutView(); }
  function updateTexts() { document.querySelectorAll('[data-i18n]').forEach(el => { el.innerText = t(el.dataset.i18n); }); }

  function updatePinoutView() {
      const sel = document.getElementById('targetSelect'); if(!sel) return;
      const tg = targets[sel.value];
      let html = `<div class="pin-row header"><span>ESP32</span><span>TARGET</span></div>`;
      const row = (l,p,t) => `<div class="pin-row"><span class="esp-pin">GPIO ${p} (${l})</span><span class="target-pin">${t}</span></div>`;
      html += row('Data',espPins.data,tg.map.data) + row('Clock',espPins.clk,tg.map.clk) + row('Reset',espPins.rst,tg.map.rst);
      html += row('GND','GND',tg.map.gnd) + row('3.3V','3.3V',tg.map.vcc);
      document.getElementById('pinoutContainer').innerHTML = html;
  }

  // --- LOGGING SYSTEM (DOM BASIERT) ---
  function log(msg) {
      const now = new Date();
      const timeSimple = now.toLocaleTimeString(); 
      const timeDetail = timeSimple + "." + String(now.getMilliseconds()).padStart(3, '0');

      // Helper zum Anhängen einer Zeile
      const appendLine = (parentId, text, isBusyUpdate) => {
          const el = document.getElementById(parentId);
          if(!el) return;

          // Prüfen, ob die letzte Zeile ein "BUSY" Update war
          const lastDiv = el.lastElementChild;
          
          // Fall: Letzte Zeile aktualisieren (um Spam zu vermeiden)
          if (isBusyUpdate && lastDiv && lastDiv.innerText.includes("BUSY")) {
              lastDiv.innerText = text;
          } 
          // Fall: Neue Zeile anfügen
          else {
              const div = document.createElement('div');
              div.innerText = text;
              div.style.borderBottom = "1px solid #111"; // Leichte Trennlinie zur Lesbarkeit
              div.style.padding = "2px 0";
              el.appendChild(div);
          }
          
          // Autoscroll
          el.scrollTop = el.scrollHeight;
      };

      // 1. Kleines Log (Flasher Tab)
      appendLine('log', `[${timeSimple}] ${msg}`, msg.includes("BUSY"));

      // 2. Großes Log (Console Tab)
      appendLine('log_mirror', `[${timeDetail}] ${msg}`, false); // false = hier wollen wir alles sehen, auch Zwischenschritte
  }
  
  function clearLog(target) { 
      const clearEl = (id) => {
          let el = document.getElementById(id);
          if(el) el.innerHTML = ""; // Inhalt komplett löschen
      };

      if (target === 'log') {
          clearEl('log');
          log("--- Cleared ---"); // Kleiner Hinweis
      } else {
          clearEl('log');
          clearEl('log_mirror');
          lastLogMsg = ""; 
          // Kleinen Hinweis im großen Log einfügen
          let el = document.getElementById('log_mirror');
          if(el) {
               let div = document.createElement('div');
               div.innerText = "Log cleared.";
               el.appendChild(div);
          }
      }
  }

  function downloadLog() {
      const el = document.getElementById('log_mirror');
      if(!el) return;
      
      // innerText holt sich automatisch die Zeilenumbrüche der Divs
      const logContent = el.innerText;
      const blob = new Blob([logContent], { type: 'text/plain' });
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      
      const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
      a.href = url;
      a.download = `cc_flasher_log_${timestamp}.txt`;
      document.body.appendChild(a);
      a.click();
      
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);
  }
  
  function toggleAllButtons(disabled) { 
      // Buttons im Flasher Tab
      document.querySelectorAll('#flasher button').forEach(btn => btn.disabled = disabled); 
      // Tabs sperren
      document.querySelectorAll('.tab').forEach(tab => {
          if(disabled) tab.classList.add('disabled'); else tab.classList.remove('disabled');
      });
  }

  function cmd(action) { toggleAllButtons(true); log("CMD: " + action); fetch('/api/' + action).then(r=>r.text()).then(txt=>{ log("RES: " + txt); toggleAllButtons(false); }).catch(e=>{ log("Err: "+e); toggleAllButtons(false); }); }

  function autoConnect() { fetch('/api/init').then(r=>r.text()).then(txt => { getChipInfo(); }).catch(e => log("Auto-Connect: No response.")); }

  function getChipInfo() {
    toggleAllButtons(true); 
    document.getElementById('infoTableContainer').innerHTML = "<p style='color:#888'>Reading Chip...</p>";
    document.getElementById('hexPreview').style.display = 'none';
    
    fetch('/api/info').then(r=>r.json()).then(data => {
        let lockCol = data.locked ? "#ff1744" : "#00c853";
        let statusText = data.locked ? "LOCKED" : "Verbunden";
        let badge = document.getElementById('connStatus');
        if(badge) { badge.innerText = statusText; badge.style.background = lockCol; }

        let html = `<table>
            <tr><th>Status</th><td style="color:${lockCol}"><b>${statusText}</b></td></tr>
            <tr><th>Model</th><td><b>${data.model}</b></td></tr>
            <tr><th>Rev</th><td>${data.rev}</td></tr>
            <tr><th>Flash</th><td>${data.flash}</td></tr>
            <tr><th>MAC</th><td>${data.mac}</td></tr>
            </table>`;
        document.getElementById('infoTableContainer').innerHTML = html;
        
        if(data.preview && data.preview !== "LOCKED") {
            let raw = data.preview.trim().split(" ");
            let formatted = "<div style='color:#00e676; margin-bottom:5px;'>Flash Preview:</div>";
            for(let i=0; i<raw.length; i+=16) {
                let addr = (i).toString(16).toUpperCase().padStart(4, '0');
                formatted += `<span class="hex-addr">${addr}:</span>${raw.slice(i, i+16).join(" ")}\n`;
            }
            let hp = document.getElementById('hexPreview'); hp.innerHTML = formatted; hp.style.display = 'block';
        }
        
        toggleAllButtons(false);
        if(data.locked) {
             ['btnFlash','btnVerify','btnDump'].forEach(id => { let b=document.getElementById(id); if(b) b.disabled=true; });
             log("INFO: Chip is LOCKED.");
        }
    }).catch(e=>{ 
        document.getElementById('infoTableContainer').innerHTML = "<p style='color:#ff5252'>Keine Antwort.</p>";
        
        // set Header status to OFFLINE
        let badge = document.getElementById('connStatus');
        if(badge) { 
            badge.innerText = "OFFLINE"; 
            badge.style.background = "#555"; // grey/dark
        }
        
        toggleAllButtons(false); 
    });
  }

  function dumpFirmware() {
    log("Starting Dump..."); toggleAllButtons(true);
    document.getElementById('dumpProgCont').style.display = 'block'; 
    document.getElementById('dumpProgBar').style.width = '0%';
    fetch('/api/start_dump').then(r=>r.text()).then(t => { log(t); lastLogMsg = ""; pollStatus('DUMP'); }).catch(e=>{ log("Err: "+e); resetUI(); });
  }

  function lockChip() {
    if(confirm(t('msg_lock_warn'))) {
      toggleAllButtons(true); log("CMD: lock_chip");
      fetch('/api/lock_chip').then(r => r.text()).then(msg => { log(msg); setTimeout(() => { fetch('/api/init').then(r => getChipInfo()); }, 1500); }).catch(e => { log("Err: " + e); resetUI(); });
    }
  }

  function eraseChip() {
    if(confirm(t('msg_erase_warn'))) {
      toggleAllButtons(true); log("CMD: erase_chip");
      fetch('/api/erase_chip').then(r => r.text()).then(msg => { log(msg); setTimeout(() => { fetch('/api/init').then(r => getChipInfo()); }, 1500); }).catch(e => { log("Err: " + e); resetUI(); });
    }
  }

  function triggerUpload(action) { currentAction = action; document.getElementById('hiddenFileInput').click(); }
  function onFileSelected(input) { if (input.files && input.files[0]) startUploadProcess(input.files[0]); input.value = ''; }

  function startUploadProcess(file) {
    let fd = new FormData(); fd.append("file", file);
    toggleAllButtons(true);
    let fpc = document.getElementById('flashProgCont'); fpc.style.display = 'block';
    let fpb = document.getElementById('flashProgBar'); fpb.style.width = '0%'; fpb.style.backgroundColor = '#00c853';
    document.getElementById('flashStatusText').style.display = 'block';
    document.getElementById('flashStatusText').innerText = t('stat_upload');
    
    log(`Upload: ${file.name}`);
    let xhr = new XMLHttpRequest(); xhr.open("POST", "/upload", true);
    xhr.upload.onprogress = function(e) { if (e.lengthComputable) fpb.style.width = ((e.loaded / e.total) * 100) + '%'; };
    xhr.onload = function() {
      if (xhr.status === 200) {
        fpb.style.width = '0%'; fpb.style.backgroundColor = '#29b6f6';
        let api = (currentAction === 'FLASH') ? '/api/start_flash' : '/api/start_verify';
        fetch(api).then(r => r.text()).then(t => { log(t); lastLogMsg = ""; pollStatus('FLASH'); });
      } else { log("Upload Failed"); resetUI(); }
    };
    xhr.send(fd);
  }

  function pollStatus(mode) {
    let iv = setInterval(() => {
        fetch('/api/status').then(r => r.json()).then(d => {
            let dpb = document.getElementById('dumpProgBar');
            let fpb = document.getElementById('flashProgBar');
            let fst = document.getElementById('flashStatusText');
            if(mode === 'DUMP' && dpb) dpb.style.width = d.pct + '%';
            else { 
                if(fpb) fpb.style.width = d.pct + '%'; 
                if(fst) fst.innerText = d.msg + " (" + d.pct + "%)";
            }
            if(d.msg !== lastLogMsg) { lastLogMsg = d.msg; if(d.msg && d.msg !== "System bereit.") log(d.msg); }
            if(d.msg === "DUMP_READY") {
                clearInterval(iv); log("Download started..."); window.location.href = "/download/dump.bin"; setTimeout(resetUI, 2000);
            } 
            else if(!d.msg.startsWith("BUSY")) {
                clearInterval(iv);
                if(d.msg.includes("Erfolg") && mode !== 'DUMP') { if(fpb) { fpb.style.width = '100%'; fpb.style.backgroundColor = '#00c853'; } }
                else if (d.msg.includes("Error") || d.msg.includes("Mismatch")) { if(fpb) fpb.style.backgroundColor = '#ff1744'; }
                setTimeout(resetUI, 2000);
            }
        }).catch(e => { clearInterval(iv); resetUI(); });
    }, 250);
  }

  function resetUI() {
    toggleAllButtons(false);
    document.getElementById('flashProgCont').style.display = 'none'; 
    document.getElementById('flashStatusText').style.display = 'none';
    document.getElementById('dumpProgCont').style.display = 'none';
  }

  function startDebugPoll() { refreshDebug(); if(!debugTimer) debugTimer = setInterval(refreshDebug, 1500); }
  function stopDebugPoll() { if(debugTimer) { clearInterval(debugTimer); debugTimer = null; } }
  function debugCmd(c) { fetch('/api/debug/'+c).then(r=>r.text()).then(t=>{log("DBG: "+t); setTimeout(refreshDebug, 400); }); }
  
  function refreshDebug() {
      fetch('/api/debug/status').then(r=>r.json()).then(s => {
          let badge = document.getElementById('cpuStateBadge');
          if(badge) {
            if(s.halted) { badge.innerText = "HALTED"; badge.className = "status-badge halt"; loadRegisters(); } 
            else { badge.innerText = "RUNNING"; badge.className = "status-badge run"; }
          }
      }).catch(e=>{});
  }

  function loadRegisters() {
      fetch('/api/debug/registers').then(r=>r.json()).then(regs => {
          // DPTR Berechnung (wie gehabt)
          let dptrVal = 0;
          // Fallback falls DPTR als String kommt oder aus DPH/DPL zusammengesetzt werden muss
          if(regs.DPTR) dptrVal = parseInt(regs.DPTR, 16);
          
          // Helper zum Setzen der Werte
          const setR = (id, val) => { 
              let el = document.getElementById('r_'+id); 
              if(el && val) el.innerText = val; 
          };

          // NEU: PC setzen
          setR('PC', regs.PC);

          // Standard Register
          setR('ACC', regs.ACC); 
          setR('B', regs.B); 
          setR('PSW', regs.PSW); 
          setR('SP', regs.SP);
          
          // GPIOs
          setR('P0', regs.P0); 
          setR('P1', regs.P1); 
          setR('P2', regs.P2);
          
          // DPH / DPL einzeln (falls im JSON) oder berechnet
          if(regs.DPH) setR('DPH', regs.DPH);
          if(regs.DPL) setR('DPL', regs.DPL);

          // NEU: R0-R7 Array verarbeiten
          if(regs.R && Array.isArray(regs.R)) {
              for(let i=0; i<8; i++) {
                  setR('R'+i, regs.R[i]);
              }
          }
      }).catch(e=>{ console.log("Reg Error", e); });
  }
  
  function jumpTo(addrStr) {
    document.getElementById('memAddr').value = addrStr;
    loadMem();
  }

  function loadMem() {
      let addr = document.getElementById('memAddr').value; let len = document.getElementById('memLen').value;
      // FIX: Use translation for loading message
      document.getElementById('hexView').innerHTML = "<div style='text-align:center; color:#666;'>" + t('msg_loading') + "</div>";
      fetch(`/api/debug/mem?addr=${addr}&len=${len}`).then(r=>r.text()).then(hex => { renderHex(addr, hex); });
  }
  
  function renderHex(startAddrStr, hexStr) {
    // 1. Hex-String in Byte-Array wandeln
    let bytes = [];
    for(let i=0; i<hexStr.length; i+=2) {
        bytes.push(parseInt(hexStr.substr(i, 2), 16));
    }
    let startAddr = parseInt(startAddrStr, 16);

    // Aktuellen PC holen (für Highlighting)
    let currentPC = -1;
    let pcEl = document.getElementById('r_PC');
    if(pcEl && pcEl.innerText !== '--') {
        currentPC = parseInt(pcEl.innerText, 16);
    }

    let html = "";
    
    if (viewMode === 'hex') {
        // --- KLASSISCHE HEX ANSICHT ---
        for(let i=0; i < bytes.length; i+=16) { 
             let chunk = bytes.slice(i, i+16);
             let currentAddr = (startAddr + i).toString(16).toUpperCase().padStart(4, '0');
             let hexBytes = chunk.map(b => b.toString(16).toUpperCase().padStart(2,'0')).join(" ");
             let asciiStr = chunk.map(b => (b>=32 && b<=126) ? String.fromCharCode(b) : ".").join("");
             // HTML Escape für ASCII
             asciiStr = asciiStr.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
             
             html += `<div class="hv-row"><span class="hv-addr">0x${currentAddr}</span><span class="hv-bytes" style="width:300px; display:inline-block;">${hexBytes}</span><span class="hv-ascii">${asciiStr}</span></div>`;
        }
    } else {
        // --- DISASSEMBLY ANSICHT ---
        let lines = disassembleBlock(startAddr, bytes);
        
        lines.forEach(line => {
            // Highlighting: Ist das der aktuelle PC?
            let style = "";
            let marker = "&nbsp;&nbsp;";
            if(line.addr === currentPC) {
                style = "background:#2e7d32; color:#fff;"; // Grüne Hervorhebung
                marker = "&#10148; "; // Pfeil
            }
            // --- NEU: Hyperlinks für Adressen (LCALL 0x1234 -> Link) ---
            // Regex sucht nach "0x" gefolgt von 4 Hex-Ziffern
            let asmFormatted = line.asm.replace(/(0x[0-9A-F]{4})/g, function(match) {
                // match ist z.B. "0xF123" -> wir brauchen "F123" für die Funktion
                let rawAddr = match.substring(2); 
                return `<span class="asm-link" onclick="jumpTo('${rawAddr}')">${match}</span>`;
            });
            // -----------------------------------------------------------

            html += `<div class="hv-row" style="${style}">
                <span style="color:#888; margin-right:5px; user-select:none;">${marker}</span>
                <span class="hv-addr" style="width:60px; display:inline-block; ${line.addr === currentPC ? 'color:#fff' : ''}">${line.addrStr}:</span>
                <span class="hv-bytes" style="color:#aaa; width:100px; display:inline-block;">${line.hex}</span>
                <!-- Hier nutzen wir jetzt asmFormatted statt line.asm -->
                <span class="hv-asm" style="color:#81d4fa; font-weight:bold;">${asmFormatted}</span>
            </div>`;
        });
    }
    
    document.getElementById('hexView').innerHTML = html;
  }
  function setBp() {
      let addr = document.getElementById('bpAddr').value;
      if(!addr) return;
      debugCmd('bp?addr=' + addr);
      // Visuelles Feedback
      document.getElementById('bpAddr').style.background = '#3a1c1c'; // Dunkelrot hinterlegen
  }

  function clearBp() {
      debugCmd('bp?addr=off');
      document.getElementById('bpAddr').value = '';
      document.getElementById('bpAddr').style.background = '#111'; // Reset
  }

  function toggleViewMode() {
    viewMode = (viewMode === 'hex') ? 'asm' : 'hex';
    loadMem(); // Neu laden / rendern
  }

)rawliteral";