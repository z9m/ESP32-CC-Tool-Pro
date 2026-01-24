#pragma once
#include <Arduino.h>

// Initialization (Mutex, etc.)
void initFlasherController();

// Status Check for API
String getStatusJSON();
bool isSystemBusy();

// Start background tasks
// Returns: true = Task started, false = System busy
bool startDumpTask();
bool startFlashTask();
bool startVerifyTask();

// Direct Actions (Blocking or fast)
void actionLockChip(void (*onSuccess)());
bool actionEraseChip();