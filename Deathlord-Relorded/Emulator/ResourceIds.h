#pragma once

// Numeric IDs the emulator core uses to ask the frontend for ROM blobs and
// bitmap firmware. Originally Win32 RT_RCDATA / RT_BITMAP IDs in
// resource/resource.h; here they're just integer keys the NacFrame maps to
// files under Resources/.

#define IDR_HDDRVR_FW                   119
#define IDR_APPLE2E_ENHANCED_ROM        130
#define IDR_APPLE2E_ENHANCED_VIDEO_ROM  131
#define IDR_MOCKINGBOARD_D_FW           135
#define IDR_TKCLOCK_FW                  148
#define IDR_HDDRVR_V2_FW                155
#define IDR_HDC_SMARTPORT_FW            156
