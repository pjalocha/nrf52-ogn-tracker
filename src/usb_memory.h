#pragma once

// USB memory mode is deliberately one-way until reset or repower.  The
// tracker gives the external flash exclusively to USB mass storage.
bool USBMemory_IsActive(void);
bool USBMemory_Enter(void);
