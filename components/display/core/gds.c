/**
 * Copyright (c) 2017-2018 Tara Keeling
 *				 2020 Philippe G. 
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "gds.h"
#include "gds_private.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#else
#define LEDC_SPEED_MODE LEDC_HIGH_SPEED_MODE
#endif                

static struct GDS_Device Display;
static struct GDS_BacklightPWM PWMConfig;

static char TAG[] = "gds";

struct GDS_Device* GDS_AutoDetect( char *Driver, GDS_DetectFunc* DetectFunc[], struct GDS_BacklightPWM* PWM ) {
	if (!Driver) return NULL;
	if (PWM) PWMConfig = *PWM;
	
	for (int i = 0; DetectFunc[i]; i++) {
		if (DetectFunc[i](Driver, &Display)) {
			if (PWM && PWM->Init) {
				ledc_timer_config_t PWMTimer = {
						.duty_resolution = LEDC_TIMER_13_BIT,
						.freq_hz = 5000,                   
						.speed_mode = LEDC_SPEED_MODE,
						.timer_num = PWMConfig.Timer,
					};
				ledc_timer_config(&PWMTimer);
			}	
			ESP_LOGD(TAG, "Detected driver %p with PWM %d", &Display, PWM ? PWM->Init : 0);			
			return &Display;
		}	
	}
	
	return NULL;
}

void GDS_ClearExt(struct GDS_Device* Device, bool full, ...) {
	bool commit = true;
	
	if (full) {
		GDS_Clear( Device, GDS_COLOR_BLACK ); 
	} else {
		va_list args;
		va_start(args, full);
		commit = va_arg(args, int);
		int x1 = va_arg(args, int), y1 = va_arg(args, int), x2 = va_arg(args, int), y2 = va_arg(args, int);
		if (x2 < 0) x2 = Device->Width - 1;
		if (y2 < 0) y2 = Device->Height - 1;
		GDS_ClearWindow( Device, x1, y1, x2, y2, GDS_COLOR_BLACK );
		va_end(args);
	}
	
	Device->Dirty = true;
	if (commit)	GDS_Update(Device);		
}	

void GDS_Clear( struct GDS_Device* Device, int Color ) {
	if (Color == GDS_COLOR_BLACK) memset( Device->Framebuffer, 0, Device->FramebufferSize );
	else if (Device->Depth == 1) memset( Device->Framebuffer, 0xff, Device->FramebufferSize );
	else if (Device->Depth == 4) memset( Device->Framebuffer, Color | (Color << 4), Device->FramebufferSize );
	else if (Device->Depth == 8) memset( Device->Framebuffer, Color, Device->FramebufferSize );
	else GDS_ClearWindow(Device, 0, 0, -1, -1, Color);
	Device->Dirty = true;
}

#define CLEAR_WINDOW(x1,y1,x2,y2,F,W,C,T,N)				\
	for (int y = y1; y <= y2; y++) {					\
		T *Ptr = (T*) F + (y * W + x1)*N;				\
		for (int c = (x2 - x1)*N; c-- >= 0; *Ptr++ = C);	\
	}

void GDS_ClearWindow( struct GDS_Device* Device, int x1, int y1, int x2, int y2, int Color ) {
	// -1 means up to width/height
	if (x2 < 0) x2 = Device->Width - 1;
	if (y2 < 0) y2 = Device->Height - 1;
	
	// driver can provide own optimized clear window
	if (Device->ClearWindow) {
		Device->ClearWindow( Device, x1, y1, x2, y2, Color );
	} else if (Device->Depth == 1) {
		// single shot if we erase all screen
		if (x2 - x1 == Device->Width - 1 && y2 - y1 == Device->Height - 1) {
			memset( Device->Framebuffer, Color == GDS_COLOR_BLACK ? 0 : 0xff, Device->FramebufferSize );
		} else {
			uint8_t _Color = Color == GDS_COLOR_BLACK ? 0: 0xff;
			uint8_t Width = Device->Width >> 3;
			uint8_t *optr = Device->Framebuffer;
			// try to do byte processing as much as possible
			for (int r = y1; r <= y2;) {
				int c = x1;
				// for a row that is not on a boundary, no optimization possible
				while (r & 0x07 && r <= y2) {
					for (c = x1; c <= x2; c++) Device->DrawPixelFast( Device, c, r, Color );
					r++;
				}
				// go fast if we have more than 8 lines to write
				if (r + 8 <= y2) {
					memset(optr + Width * r + x1, _Color, x2 - x1 + 1);
					r += 8;
				} else while (r <= y2) {
					for (c = x1; c <= x2; c++) Device->DrawPixelFast( Device, c, r, Color );
					r++;
				}
			}
		}
	} if (Device->Depth == 4) {
		if (x2 - x1 == Device->Width - 1 && y2 - y1 == Device->Height - 1) {
			// we assume color is 0..15
			memset( Device->Framebuffer, Color | (Color << 4), Device->FramebufferSize );
		} else {
			uint8_t _Color = Color | (Color << 4);
			int Width = Device->Width;
			uint8_t *optr = Device->Framebuffer;
			// try to do byte processing as much as possible
			for (int r = y1; r <= y2; r++) {
				int c = x1;
				if (c & 0x01) Device->DrawPixelFast( Device, c++, r, Color);
				int chunk = (x2 - c + 1) >> 1;
				memset(optr + ((r * Width + c)  >> 1), _Color, chunk);
				if (c + chunk <= x2) Device->DrawPixelFast( Device, x2, r, Color);
			}
		}	
	} else if (Device->Depth == 8) {
		CLEAR_WINDOW(x1,y1,x2,y2,Device->Framebuffer,Device->Width,Color,uint8_t,1);
	} else if (Device->Depth == 16) {
		CLEAR_WINDOW(x1,y1,x2,y2,Device->Framebuffer,Device->Width,Color,uint16_t,1);
	} else if (Device->Depth == 24) {
		CLEAR_WINDOW(x1,y1,x2,y2,Device->Framebuffer,Device->Width,Color,uint8_t,3);
	} else {
		for (int y = y1; y <= y2; y++) {
			for (int x = x1; x <= x2; x++) {
				Device->DrawPixelFast( Device, x, y, Color);
			}
		}
	}
	
	// make sure diplay will do update
	Device->Dirty = true;
}

void GDS_Update( struct GDS_Device* Device ) {
	if (Device->Dirty) Device->Update( Device );
	Device->Dirty = false;
}

bool GDS_Reset( struct GDS_Device* Device ) {
	if ( Device->RSTPin >= 0 ) {
		gpio_set_level( Device->RSTPin, 0 );
		vTaskDelay( pdMS_TO_TICKS( 100 ) );
        gpio_set_level( Device->RSTPin, 1 );
    }
    return true;
}

static void IRAM_ATTR DrawPixel1Fast( struct GDS_Device* Device, int X, int Y, int Color ) {
    uint32_t YBit = ( Y & 0x07 );
    uint8_t* FBOffset;

    /* 
     * We only need to modify the Y coordinate since the pitch
     * of the screen is the same as the width.
     * Dividing Y by 8 gives us which row the pixel is in but not
     * the bit position.
     */
    Y>>= 3;

    FBOffset = Device->Framebuffer + ( ( Y * Device->Width ) + X );

    if ( Color == GDS_COLOR_XOR ) {
        *FBOffset ^= BIT( YBit );
    } else {
        *FBOffset = ( Color == GDS_COLOR_BLACK ) ? *FBOffset & ~BIT( YBit ) : *FBOffset | BIT( YBit );
    }
}

static void IRAM_ATTR DrawPixel4Fast( struct GDS_Device* Device, int X, int Y, int Color ) {
	uint8_t* FBOffset = Device->Framebuffer + ( (Y * Device->Width >> 1) + (X >> 1));
	*FBOffset = X & 0x01 ? (*FBOffset & 0x0f) | ((Color & 0x0f) << 4) : ((*FBOffset & 0xf0) | (Color & 0x0f));
}

static void IRAM_ATTR DrawPixel4FastHigh( struct GDS_Device* Device, int X, int Y, int Color ) {
    uint8_t* FBOffset = Device->Framebuffer + ( (Y * Device->Width >> 1) + (X >> 1));
	*FBOffset = X & 0x01 ? ((*FBOffset & 0xf0) | (Color & 0x0f)) : (*FBOffset & 0x0f) | ((Color & 0x0f) << 4);
}

static void IRAM_ATTR DrawPixel8Fast( struct GDS_Device* Device, int X, int Y, int Color ) {
	Device->Framebuffer[Y * Device->Width + X] = Color;
}

// assumes that Color is 16 bits R..RG..GB..B from MSB to LSB and FB wants 1st serialized byte to start with R
static void IRAM_ATTR DrawPixel16Fast( struct GDS_Device* Device, int X, int Y, int Color ) {
	uint16_t* FBOffset = (uint16_t*) Device->Framebuffer + Y * Device->Width + X;
	*FBOffset = __builtin_bswap16(Color);
}

// assumes that Color is 18 bits RGB from MSB to LSB RRRRRRGGGGGGBBBBBB, so byte[0] is B 
// FB is 3-bytes packets and starts with R for serialization so 0,1,2 ... = xxRRRRRR xxGGGGGG xxBBBBBB 
static void IRAM_ATTR DrawPixel18Fast( struct GDS_Device* Device, int X, int Y, int Color ) {
	uint8_t* FBOffset = Device->Framebuffer + (Y * Device->Width + X) * 3;
	*FBOffset++ = Color >> 12; *FBOffset++ = (Color >> 6) & 0x3f; *FBOffset = Color & 0x3f;
}

// assumes that Color is 24 bits RGB from MSB to LSB RRRRRRRRGGGGGGGGBBBBBBBB, so byte[0] is B 
// FB is 3-bytes packets and starts with R for serialization so 0,1,2 ... = RRRRRRRR GGGGGGGG BBBBBBBB 
static void IRAM_ATTR DrawPixel24Fast( struct GDS_Device* Device, int X, int Y, int Color ) {
	uint8_t* FBOffset = Device->Framebuffer + (Y * Device->Width + X) * 3;
	*FBOffset++ = Color >> 16; *FBOffset++ = Color >> 8; *FBOffset = Color;
}

bool GDS_Init( struct GDS_Device* Device ) {
	
	if (Device->Depth > 8) Device->FramebufferSize = Device->Width * Device->Height * ((8 + Device->Depth - 1) / 8);
	else Device->FramebufferSize = (Device->Width * Device->Height) / (8 / Device->Depth);
    
    // set the proper DrawPixel function if not already set by driver
    if (!Device->DrawPixelFast) {
        if (Device->Depth == 1) Device->DrawPixelFast = DrawPixel1Fast;
        else if (Device->Depth == 4 && Device->HighNibble) Device->DrawPixelFast = DrawPixel4FastHigh;
        else if (Device->Depth == 4) Device->DrawPixelFast = DrawPixel4Fast;
        else if (Device->Depth == 8) Device->DrawPixelFast = DrawPixel8Fast;	
        else if (Device->Depth == 16) Device->DrawPixelFast = DrawPixel16Fast;	
        else if (Device->Depth == 24 && Device->Mode == GDS_RGB666) Device->DrawPixelFast = DrawPixel18Fast;	
        else if (Device->Depth == 24 && Device->Mode == GDS_RGB888) Device->DrawPixelFast = DrawPixel24Fast;	
    }	
	
	// allocate FB unless explicitely asked not to
	if (!(Device->Alloc & GDS_ALLOC_NONE)) {
		if ((Device->Alloc & GDS_ALLOC_IRAM) || ((Device->Alloc & GDS_ALLOC_IRAM_SPI) && Device->IF == GDS_IF_SPI)) {
			Device->Framebuffer = heap_caps_calloc( 1, Device->FramebufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA );
		} else {
			Device->Framebuffer = calloc( 1, Device->FramebufferSize );
		}	
		NullCheck( Device->Framebuffer, return false );
	}	
	
	if (Device->Backlight.Pin >= 0) {
		Device->Backlight.Channel = PWMConfig.Channel++;
		Device->Backlight.PWM = PWMConfig.Max - 1;

		ledc_channel_config_t PWMChannel = {
            .channel    = Device->Backlight.Channel,
            .duty       = Device->Backlight.PWM,
            .gpio_num   = Device->Backlight.Pin,
            .speed_mode = LEDC_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = PWMConfig.Timer,
        };
		
		ledc_channel_config(&PWMChannel);
	}
	
	bool Res = Device->Init( Device );
	if (!Res && Device->Framebuffer) free(Device->Framebuffer);
	return Res;
}

int GDS_GrayMap( struct GDS_Device* Device, uint8_t Level) {
	switch(Device->Mode) {
		case GDS_MONO: return Level;
		case GDS_GRAYSCALE: return Level >> (8 - Device->Depth);
		case GDS_RGB332:
			Level >>= 5;	
			return (Level << 6) | (Level << 3) | (Level >> 1);
		case GDS_RGB444:	
			Level >>= 4;
			return (Level << 8) | (Level << 4) | Level;
		case GDS_RGB555:	
			Level >>= 3;
			return (Level << 10) | (Level << 5) | Level;			
		case GDS_RGB565:	
			Level >>= 2;
			return ((Level & ~0x01) << 10) | (Level << 5) | (Level >> 1);						
		case GDS_RGB666:	
			Level >>= 2;
			return (Level << 12) | (Level << 6) | Level;									
		case GDS_RGB888:	
			return (Level << 16) | (Level << 8) | Level;												
	}
	
	return -1;
}

void GDS_SetContrast( struct GDS_Device* Device, uint8_t Contrast ) { 
	if (Device->SetContrast) Device->SetContrast( Device, Contrast ); 
	else if (Device->Backlight.Pin >= 0) {
		Device->Backlight.PWM = PWMConfig.Max * powf(Contrast / 255.0, 3);
		ledc_set_duty( LEDC_SPEED_MODE, Device->Backlight.Channel, Device->Backlight.PWM );
		ledc_update_duty( LEDC_SPEED_MODE, Device->Backlight.Channel );		
	}
}

void GDS_SetLayout( struct GDS_Device* Device, struct GDS_Layout *Layout ) { if (Device->SetLayout) Device->SetLayout( Device, Layout ); }
void GDS_SetDirty( struct GDS_Device* Device ) { Device->Dirty = true; }
int	 GDS_GetWidth( struct GDS_Device* Device ) { return Device ? Device->Width : 0; }
void GDS_SetTextWidth( struct GDS_Device* Device, int TextWidth ) { Device->TextWidth = Device && TextWidth && TextWidth < Device->Width ? TextWidth : Device->Width; }
int	 GDS_GetHeight( struct GDS_Device* Device ) { return Device ? Device->Height : 0; }
int	 GDS_GetDepth( struct GDS_Device* Device ) { return Device ? Device->Depth : 0; }
int	 GDS_GetMode( struct GDS_Device* Device ) { return Device ? Device->Mode : 0; }
void GDS_DisplayOn( struct GDS_Device* Device ) { if (Device->DisplayOn) Device->DisplayOn( Device ); }
void GDS_DisplayOff( struct GDS_Device* Device ) { if (Device->DisplayOff) Device->DisplayOff( Device ); }