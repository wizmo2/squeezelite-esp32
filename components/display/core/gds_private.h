/* 
 * (c) Philippe G. 2019, philippe_44@outlook.com
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 * 
 */
 
#ifndef _GDS_PRIVATE_H_
#define _GDS_PRIVATE_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_attr.h"
#include "gds.h"
#include "gds_err.h"

#define GDS_ALLOC_NONE		0x80
#define GDS_ALLOC_IRAM		0x01
#define GDS_ALLOC_IRAM_SPI	0x02

#define GDS_CLIPDEBUG_NONE 0
#define GDS_CLIPDEBUG_WARNING 1
#define GDS_CLIPDEBUG_ERROR 2

#if CONFIG_GDS_CLIPDEBUG == GDS_CLIPDEBUG_NONE
    /*
     * Clip silently with no console output.
     */
    #define ClipDebug( x, y )
#elif CONFIG_GDS_CLIPDEBUG == GDS_CLIPDEBUG_WARNING
    /*
     * Log clipping to the console as a warning.
     */
    #define ClipDebug( x, y ) { \
        ESP_LOGW( __FUNCTION__, "Line %d: Pixel at %d, %d CLIPPED", __LINE__, x, y ); \
    }
#elif CONFIG_GDS_CLIPDEBUG == GDS_CLIPDEBUG_ERROR
    /*
     * Log clipping as an error to the console.
     * Also invokes an abort with stack trace.
     */
    #define ClipDebug( x, y ) { \
        ESP_LOGE( __FUNCTION__, "Line %d: Pixel at %d, %d CLIPPED, ABORT", __LINE__, x, y ); \
        abort( ); \
    }
#endif


#define GDS_ALWAYS_INLINE __attribute__( ( always_inline ) )

#define MAX_LINES	8

#if ! defined BIT
#define BIT( n ) ( 1 << ( n ) )
#endif

struct GDS_Device;
struct GDS_FontDef;

/*
 * These can optionally return a succeed/fail but are as of yet unused in the driver.
 */
typedef bool ( *WriteCommandProc ) ( struct GDS_Device* Device, uint8_t Command );
typedef bool ( *WriteDataProc ) ( struct GDS_Device* Device, const uint8_t* Data, size_t DataLength );

struct spi_device_t;
typedef struct spi_device_t* spi_device_handle_t;

#define GDS_IF_SPI	0
#define GDS_IF_I2C	1

struct GDS_Device {
	uint8_t IF;
	int8_t RSTPin;
	struct {
		int8_t Pin, Channel;
		int PWM;	
	} Backlight;
	union {
		// I2C Specific
		struct {
			uint8_t Address;
		};
		// SPI specific
		struct {
			spi_device_handle_t SPIHandle;
			int8_t CSPin;
		};
	};	
	
    // cooked text mode
	struct {
		int16_t Y, Space;
		const struct GDS_FontDef* Font;
	} Lines[MAX_LINES];
	
	uint16_t Width, TextWidth;
    uint16_t Height;
	uint8_t Depth, Mode;
    bool HighNibble;
	
	uint8_t	Alloc;	
	uint8_t* Framebuffer;
    uint32_t FramebufferSize;
	bool Dirty;

	// default fonts when using direct draw	
	const struct GDS_FontDef* Font;
    bool FontForceProportional;
    bool FontForceMonospace;

	// various driver-specific method
	// must always provide 
	bool (*Init)( struct GDS_Device* Device);
	void (*Update)( struct GDS_Device* Device );
	// may provide if supported
	void (*SetContrast)( struct GDS_Device* Device, uint8_t Contrast );
	void (*DisplayOn)( struct GDS_Device* Device );
	void (*DisplayOff)( struct GDS_Device* Device );
	void (*SetLayout)( struct GDS_Device* Device, struct GDS_Layout *Layout );
	// must provide for depth other than 1 (vertical) and 4 (may provide for optimization)
	void (*DrawPixelFast)( struct GDS_Device* Device, int X, int Y, int Color );
	void (*DrawBitmapCBR)(struct GDS_Device* Device, uint8_t *Data, int Width, int Height, int Color );
	// may provide for optimization
	void (*DrawRGB)( struct GDS_Device* Device, uint8_t *Image,int x, int y, int Width, int Height, int RGB_Mode );
	void (*ClearWindow)( struct GDS_Device* Device, int x1, int y1, int x2, int y2, int Color );
	// may provide for tweaking
	void (*SPIParams)(int Speed, uint8_t *mode, uint16_t *CS_pre, uint8_t *CS_post);
		    
	// interface-specific methods	
    WriteCommandProc WriteCommand;
    WriteDataProc WriteData;

	// 32 bytes for whatever the driver wants (should be aligned as it's 32 bits)	
	uint32_t Private[8];
};

bool GDS_Reset( struct GDS_Device* Device );
bool GDS_Init( struct GDS_Device* Device );

static inline bool IsPixelVisible( struct GDS_Device* Device, int x, int y )  {
    bool Result = (
        ( x >= 0 ) &&
        ( x < Device->Width ) &&
        ( y >= 0 ) &&
        ( y < Device->Height )
    ) ? true : false;

#if CONFIG_GDS_CLIPDEBUG > 0
    if ( Result == false ) {
        ClipDebug( x, y );
    }
#endif

    return Result;
}

static inline void IRAM_ATTR DrawPixel( struct GDS_Device* Device, int x, int y, int Color ) {
    if ( IsPixelVisible( Device, x, y ) == true ) {
        Device->DrawPixelFast( Device, x, y, Color );
    }
}

#endif