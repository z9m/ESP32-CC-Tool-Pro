#ifndef WEB_INDEX_H
#define WEB_INDEX_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <title>TI CC Flasher PRO</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    /* --- THEME & BASICS --- */
    :root {
      --bg: #121212; --card: #1e1e1e; --text: #e0e0e0; 
      --accent: #29b6f6; --accent-hover: #039be5;
      --border: #333; --success: #00c853; --danger: #ff1744; --warn: #ff9800;
    }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace; background: var(--bg); color: var(--text); margin: 0; padding: 0; display: flex; flex-direction: column; height: 100vh; overflow: hidden; }
    
    /* SCROLLBARS */
    ::-webkit-scrollbar { width: 10px; height: 10px; }
    ::-webkit-scrollbar-track { background: var(--bg); }
    ::-webkit-scrollbar-thumb { background: #444; border-radius: 5px; border: 2px solid var(--bg); }

    /* HEADER */
    header { background: #181818; padding: 10px 20px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; }
    h1 { margin: 0; font-size: 1.2rem; color: var(--accent); letter-spacing: 1px; font-weight:bold; }
    
    /* TABS */
    .tabs { display: flex; background: #252525; border-bottom: 1px solid var(--border); }
    .tab { padding: 12px 25px; cursor: pointer; color: #888; font-weight: bold; border-right: 1px solid var(--border); transition: 0.2s; }
    .tab:hover { background: #333; color: #fff; }
    .tab.active { background: var(--card); color: var(--accent); border-bottom: 3px solid var(--accent); }
    /* Sperrt die Tabs */
    .tab.disabled { pointer-events: none; opacity: 0.5; cursor: not-allowed; background: #1a1a1a; color: #555; }

    .content-area { flex: 1; overflow-y: auto; padding: 20px; display: none; }
    .content-area.active { display: block; }

    /* CARDS & GRID */
    .flasher-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; align-items: stretch; }
    @media (min-width: 1400px) { .flasher-grid { grid-template-columns: repeat(4, 1fr); } }

    .card { background: var(--card); border: 1px solid var(--border); border-radius: 6px; padding: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.3); display: flex; flex-direction: column; }
    .card.full-width { grid-column: 1 / -1; margin-top: 20px; width: 100%; box-sizing: border-box; }
    
    h2 { font-size: 0.85rem; margin: 0 0 15px 0; padding-bottom: 8px; border-bottom: 1px solid #333; color: #fff; text-transform: uppercase; letter-spacing: 0.5px; font-weight: bold; }

    /* UI ELEMENTS */
    table { width: 100%; border-collapse: collapse; font-size: 0.9rem; }
    td, th { text-align: left; padding: 6px; border-bottom: 1px solid #2a2a2a; }
    th { color: #888; width: 80px; }
    
    .pinout-box { background: #111; border: 1px solid #333; border-radius: 4px; margin-top: auto; padding: 0; overflow: hidden; }
    .pin-row { display: flex; justify-content: space-between; padding: 8px 12px; border-bottom: 1px solid #222; font-family: monospace; font-size: 0.85rem; }
    .pin-row.header { background: #2f363d; color: var(--accent); font-weight: bold; border-bottom: 1px solid #444; }
    
    .hex-box { background: #000; color: #0f0; font-family: 'Courier New', monospace; padding: 10px; border: 1px solid #333; border-radius: 4px; margin-top: 15px; font-size: 0.8rem; white-space: pre-wrap; display: none; line-height: 1.4; flex:1; }
    .hex-addr { color: #666; margin-right: 10px; }

    .btn-group { display: flex; flex-direction: column; gap: 10px; }
    button { background: #333; color: white; border: 1px solid #555; padding: 12px 15px; cursor: pointer; border-radius: 4px; font-family: inherit; font-size: 0.85rem; font-weight: 600; text-transform: uppercase; transition: 0.2s; }
    button:hover { background: #444; border-color: #888; }
    button:disabled { opacity: 0.5; cursor: not-allowed; }
    button.primary { background: var(--accent); color: #000; border: none; }
    button.primary:hover { background: var(--accent-hover); }
    button.danger { border-color: var(--danger); color: var(--danger); background: transparent; }
    button.danger:hover { background: var(--danger); color: white; }
    button.warn { border-color: var(--warn); color: var(--warn); background: transparent; }
    button.warn:hover { background: var(--warn); color: white; }

    .prog-cont { background:#111; height:6px; border-radius:3px; overflow:hidden; margin-top: 15px; display: none; }
    .prog-bar { width:0%; height:100%; background:var(--accent); transition: width 0.2s; }
    #flashStatusText { text-align:center; font-size:0.8rem; color:#888; margin-top:8px; display: none; }
    #connStatus { font-size: 0.75rem; padding: 4px 8px; border-radius: 4px; font-weight: bold; text-transform: uppercase; color: #fff; background: #444; margin-right: 10px; }
    
    /* LOG STYLING */
    .log-view { 
        font-family: 'Courier New', monospace; 
        font-size: 0.8rem; 
        color: #00e676; 
        
        /* WICHTIG: Damit lange Wörter (Hex-Strings) umbrechen */
        word-break: break-all;       
        
        overflow-y: auto; 
        background: #000; 
        padding: 10px; 
        border: 1px solid #333; 
        display: flex;             /* NEU: Flexbox sorgt für saubere Stapelung */
        flex-direction: column;    /* NEU: Untereinander */
    }
    #log { height: 150px; } /* Kleineres Log im Flasher Tab */
    #log_mirror { height: calc(100vh - 180px); } /* Großes Log im Console Tab */

    /* --- DEBUGGER STYLES (New Layout) --- */
    .debug-layout { display: flex; flex-direction: column; height: 100%; gap: 15px; }
    
    .debug-toolbar { 
        background: var(--card); border: 1px solid var(--border); padding: 10px; border-radius: 6px;
        display: flex; align-items: center; gap: 15px; flex-wrap: wrap; 
    }
    
    .status-badge { 
        padding: 6px 12px; border-radius: 4px; font-size: 0.85rem; font-weight: bold; 
        background: #333; color: #888; border: 1px solid #444; min-width: 80px; text-align: center;
    }
    .status-badge.run { background: rgba(0, 200, 83, 0.2); color: #00c853; border-color: #00c853; }
    .status-badge.halt { background: rgba(255, 23, 68, 0.2); color: #ff1744; border-color: #ff1744; }
    
    .debug-main { display: flex; flex: 1; gap: 15px; min-height: 0; }
    .debug-left { width: 240px; display: flex; flex-direction: column; gap: 15px; flex-shrink: 0; }
    .debug-right { flex: 1; display: flex; flex-direction: column; background: var(--card); border: 1px solid var(--border); border-radius: 6px; overflow: hidden; }

    .reg-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 1px; background: #333; border: 1px solid #333; }
    .reg-item { background: #1e1e1e; padding: 6px 10px; font-family: monospace; font-size: 0.9rem; display: flex; justify-content: space-between; align-items: center; }
    .reg-name { color: var(--accent); font-weight: bold; }
    .reg-val { color: #fff; }

    .hex-toolbar { padding: 8px 15px; background: #252525; border-bottom: 1px solid var(--border); display: flex; gap: 10px; align-items: center; }
    .hex-input { background: #111; border: 1px solid #444; color: white; font-family: monospace; padding: 4px 8px; font-size: 0.9rem; width: 70px; border-radius: 3px; }
    .hex-btn { padding: 4px 12px; font-size: 0.8rem; min-width: auto; }

    .hex-view { flex: 1; overflow-y: auto; background: #111; color: #ccc; font-family: 'Consolas', 'Courier New', monospace; font-size: 0.9rem; padding: 15px; line-height: 1.3; }
    .hv-row { display: flex; } .hv-row:hover { background: #222; }
    .hv-addr { color: #29b6f6; margin-right: 20px; user-select: none; } 
    .hv-bytes { color: #ce9178; margin-right: 20px; } 
    .hv-ascii { color: #00c853; }
    
    .asm-link { 
        color: #4fc3f7; 
        text-decoration: underline; 
        cursor: pointer; 
        font-weight: bold;
    }
    .asm-link:hover { 
        color: #fff; 
        background: #004d40;
        text-decoration: none;
    }

    @media (max-width: 800px) {
        .debug-main { flex-direction: column; } .debug-left { width: 100%; } .debug-right { height: 400px; }
    }
  </style>
</head>
<body>

  <header>
    <h1>CC-TOOL <span style="font-size:0.8em; color:#fff; opacity:0.5;">PRO</span></h1>
    <div style="display:flex; align-items:center;">
        <span id="connStatus">Ready</span>
        <select id="langSelect" onchange="changeLang(this.value)" style="background:#222; color:#fff; border:1px solid #444; padding:3px; border-radius:3px;">
            <option value="de">DE</option>
            <option value="en">EN</option>
            <option value="es">ES</option>
            <option value="fr">FR</option>
            <option value="it">IT</option>
            <option value="pl">PL</option>
            <option value="cs">CS</option>
            <option value="ja">JA</option>
            <option value="zh">ZH</option>
        </select>
    </div>
  </header>

  <div class="tabs">
    <div class="tab active" onclick="switchTab('flasher')" data-i18n="tab_flasher">FLASHER</div>
    <div class="tab" onclick="switchTab('debugger')" data-i18n="tab_debugger">DEBUGGER</div>
    <div class="tab" onclick="switchTab('console')" data-i18n="tab_log">LOG</div>
  </div>

  <div id="flasher" class="content-area active">
    <div class="flasher-grid">
        <div class="card">
            <h2 data-i18n="sec_ctrl">1. Steuerung</h2>
            <div class="btn-group">
                <button class="primary" onclick="cmd('init')" data-i18n="btn_init">INIT / RESET</button>
                <button class="primary" onclick="getChipInfo()" data-i18n="btn_info">INFO PAGE LESEN</button>
                <button style="background:#444;" onclick="dumpFirmware()" id="btnDump" data-i18n="btn_dump">FLASH AUSLESEN</button>
                <div id="dumpProgCont" class="prog-cont"><div id="dumpProgBar" class="prog-bar"></div></div>
                <hr style="border:0; border-top:1px solid #333; width:100%; margin:5px 0;">
                <button class="danger" onclick="lockChip()" data-i18n="btn_lock">LOCK CHIP (PROTECT)</button>
                <button class="warn" onclick="eraseChip()" data-i18n="btn_erase">CHIP LÖSCHEN (UNLOCK)</button>
            </div>
        </div>

        <div class="card">
            <h2 data-i18n="sec_details">2. Chip Details</h2>
            <div id="infoTableContainer" style="min-height:80px; display:flex; align-items:center; color:#666;">
                <span data-i18n="lbl_nodata">Keine Daten.</span>
            </div>
            <div id="hexPreview" class="hex-box"></div>
        </div>

        <div class="card">
            <h2 data-i18n="sec_pinout">4. Pinout Reference</h2>
            <select id="targetSelect" onchange="updatePinoutView()" style="width:100%; padding:8px; background:#222; color:#fff; border:1px solid #444; border-radius:4px; margin-bottom:10px;">
                <option value="std" data-i18n="opt_std">Standard CC-Debugger</option>
                <option value="chroma74" data-i18n="opt_chroma">Chroma 74 E-Ink</option>
            </select>
            <div id="pinoutContainer" class="pinout-box"></div>
        </div>

        <div class="card">
            <h2 data-i18n="sec_fw">3. Firmware Update</h2>
            <input type="file" id="hiddenFileInput" accept=".bin" style="display:none" onchange="onFileSelected(this)">
            <div class="btn-group">
                <button id="btnFlash" class="primary" style="flex:1;" onclick="triggerUpload('FLASH')" data-i18n="btn_flash">FLASHEN</button>
                <button id="btnVerify" style="flex:1; background:#444;" onclick="triggerUpload('VERIFY')" data-i18n="btn_verify">VERIFIZIEREN</button>
            </div>       
            <div id="flashProgCont" class="prog-cont"><div id="flashProgBar" class="prog-bar"></div></div>
            <p id="flashStatusText"></p>
        </div>
    </div>

    <div class="card full-width">
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;">
            <h2 style="margin:0; border:none;" data-i18n="sec_log">Protokoll</h2>
            <button onclick="clearLog('log')" style="padding:4px 8px; font-size:0.7rem;" data-i18n="btn_clear">Clear</button>
        </div>
        <div id="log" class="log-view">System bereit.</div>
    </div>
  </div>

  <div id="debugger" class="content-area" style="padding-bottom:0;">
      <div class="debug-layout">
          <div class="debug-toolbar">
              <div id="cpuStateBadge" class="status-badge">UNKNOWN</div>
              <div style="display:flex; gap:10px;">
                  <button class="danger" onclick="debugCmd('halt')" title="Halt CPU" style="padding:8px 16px; font-size:0.9rem;" data-i18n="btn_halt">&#10074;&#10074; HALT</button>
                  <button class="primary" onclick="debugCmd('resume')" title="Resume CPU" style="padding:8px 16px; font-size:0.9rem;" data-i18n="btn_resume">&#9654; RUN</button>
                  <button onclick="debugCmd('step')" title="Step Into" style="background:#444; border-color:#29b6f6; color:#29b6f6;" data-i18n="btn_step">&#8618; STEP</button>
              </div>
              <div style="flex:1;"></div>
                <div style="width:1px; height:30px; background:#444; margin:0 10px;"></div>

                <div style="display:flex; align-items:center; gap:5px;">
                    <span style="font-size:0.8rem; color:#aaa;">BP:</span>
                    <input type="text" id="bpAddr" placeholder="Addr" class="hex-input" style="width:50px; border-color:#ff4081;">
                    <button onclick="setBp()" class="hex-btn" style="border-color:#ff4081; color:#ff4081;">SET</button>
                    <button onclick="clearBp()" class="hex-btn" style="color:#888;">CLR</button>
                </div>
              <div style="flex:1;"></div>
              <button onclick="refreshDebug()" data-i18n="btn_refresh">&#x21bb; Refresh</button>
          </div>

          <div class="debug-main">
              <div class="debug-left">
                  <div class="card" style="padding:0; overflow:hidden;">
                      <h2 style="padding:10px 15px; margin:0; background:#252525; border-bottom:1px solid #333;" data-i18n="sec_registers">Registers</h2>
                      <div class="reg-grid">
                          <div class="reg-item" style="background:#2a2a2a; border-bottom:1px solid #444;">
                              <span class="reg-name" style="color:#ff4081;">PC</span>
                              <span class="reg-val" id="r_PC" style="font-weight:bold; color:#ff4081;">--</span>
                          </div>
                          <!-- Platzhalter für Layout-Symmetrie (oder leer lassen) -->
                          <div class="reg-item" style="background:#2a2a2a; border-bottom:1px solid #444;"></div>
                          <div class="reg-item"><span class="reg-name">ACC</span><span class="reg-val" id="r_ACC">--</span></div>
                          <div class="reg-item"><span class="reg-name">B</span><span class="reg-val" id="r_B">--</span></div>
                          <div class="reg-item"><span class="reg-name">PSW</span><span class="reg-val" id="r_PSW">--</span></div>
                          <div class="reg-item"><span class="reg-name">SP</span><span class="reg-val" id="r_SP">--</span></div>
                          <div class="reg-item"><span class="reg-name">DPL</span><span class="reg-val" id="r_DPL">--</span></div>
                          <div class="reg-item"><span class="reg-name">DPH</span><span class="reg-val" id="r_DPH">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R0</span><span class="reg-val" id="r_R0">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R1</span><span class="reg-val" id="r_R1">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R2</span><span class="reg-val" id="r_R2">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R3</span><span class="reg-val" id="r_R3">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R4</span><span class="reg-val" id="r_R4">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R5</span><span class="reg-val" id="r_R5">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R6</span><span class="reg-val" id="r_R6">--</span></div>
                          <div class="reg-item"><span class="reg-name" style="color:#aaa">R7</span><span class="reg-val" id="r_R7">--</span></div>
                      </div>
                  </div>
                  <div class="card" style="padding:0; overflow:hidden;">
                      <h2 style="padding:10px 15px; margin:0; background:#252525; border-bottom:1px solid #333;" data-i18n="sec_gpio">GPIO Ports</h2>
                      <div class="reg-grid">
                          <div class="reg-item"><span class="reg-name">P0</span><span class="reg-val" id="r_P0">--</span></div>
                          <div class="reg-item"><span class="reg-name">P1</span><span class="reg-val" id="r_P1">--</span></div>
                          <div class="reg-item"><span class="reg-name">P2</span><span class="reg-val" id="r_P2">--</span></div>
                      </div>
                  </div>
              </div>

              <div class="debug-right">
                  <div class="hex-toolbar">
                      <span style="font-size:0.9rem; font-weight:bold; color:#aaa;" data-i18n="sec_xdata">XDATA MEMORY</span>
                      <div style="flex:1"></div>
                      <span style="font-size:0.9rem; color:#888;">Addr: 0x</span>
                      <input type="text" id="memAddr" value="F000" class="hex-input" onkeydown="if(event.key==='Enter') loadMem()">
                      <span style="font-size:0.9rem; color:#888;" data-i18n="lbl_len">Len:</span>
                      <input type="text" id="memLen" value="256" class="hex-input" style="width:50px;" onkeydown="if(event.key==='Enter') loadMem()">
                      <button onclick="loadMem()" class="primary hex-btn" data-i18n="btn_go">GO</button>
                      <div style="width:1px; height:20px; background:#444; margin:0 10px;"></div>
                        <button onclick="toggleViewMode()" class="hex-btn" style="background:#444;" title="Switch Hex/Asm View">HEX / ASM</button>
                  </div>
                  <div id="hexView" class="hex-view">
                      <div style="color:#666; text-align:center; padding-top:50px;" data-i18n="msg_wait_debug">Waiting for debug data...</div>
                  </div>
              </div>
          </div>
      </div>
  </div>

  <div id="console" class="content-area">
      <div class="card full-width" style="height:100%;">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;">
              <h2 style="margin:0; border:none;" data-i18n="sec_log">Protokoll (Full View)</h2>
              <div>
                  <button onclick="downloadLog()" style="padding:4px 8px; font-size:0.7rem; background:#444; margin-right:5px;">&#11015; Download Log</button>
                  <button onclick="clearLog()" style="padding:4px 8px; font-size:0.7rem;" data-i18n="btn_clear">Clear All</button>
              </div>
          </div>
          <div id="log_mirror" class="log-view">System bereit.</div>
      </div>
  </div>

  <script src="/script.js"></script>
</body>
</html>
)rawliteral";

#endif