/**
 * Copyright (c) 2017-2018 Tara Keeling
 *				 2020 Philippe G.
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include "gds.h"
#include "gds_private.h"

#define SHADOW_BUFFER
#define PAGE_BLOCK	1024

#define min(a,b) (((a) < (b)) ? (a) : (b))

static char TAG[] = "SH1122";

struct PrivateSpace {
	uint8_t *iRAM, *Shadowbuffer;
	uint8_t PageSize;
};

// Functions are not declared to minimize # of lines

static void SetColumnAddress( struct GDS_Device* Device, uint8_t Start, uint8_t End ) {
    Device->WriteCommand( Device, 0x10 | (Start >> 4) );
	Device->WriteCommand( Device, 0x00 | (Start & 0x0f) );
}

static void SetRowAddress( struct GDS_Device* Device, uint8_t Start, uint8_t End ) {
	Device->WriteCommand( Device, 0xB0 );
	Device->WriteCommand( Device, Start );
}

static void Update( struct GDS_Device* Device ) {
	struct PrivateSpace *Private = (struct PrivateSpace*) Device->Private;
		
	// RAM is by columns of 4 pixels ...
	SetColumnAddress( Device, 0, Device->Width / 4 - 1);
	
#ifdef SHADOW_BUFFER
	uint16_t *optr = (uint16_t*) Private->Shadowbuffer, *iptr = (uint16_t*) Device->Framebuffer;
	bool dirty = false;
	
	for (int r = 0, page = 0; r < Device->Height; r++) {
		// look for change and update shadow (cheap optimization = width always / by 2)
		for (int c = Device->Width / 2 / 2; --c >= 0;) {
			if (*optr != *iptr) {
				dirty = true;
				*optr = *iptr;
			}
			iptr++; optr++;
		}
		
		// one line done, check for page boundary
		if (++page == Private->PageSize) {
			if (dirty) {
				SetRowAddress( Device, r - page + 1, r );
                if (Private->iRAM) {
                    memcpy(Private->iRAM, Private->Shadowbuffer + (r - page + 1) * Device->Width / 2, page * Device->Width / 2 );
                    Device->WriteData( Device, Private->iRAM, Device->Width * page / 2 );
                } else {
                    Device->WriteData( Device, Private->Shadowbuffer + (r - page + 1) * Device->Width / 2, page * Device->Width / 2);
                }    
				dirty = false;
			}	
			page = 0;
		}	
	}	
#else
    SetRowAddress( Device, 0, Device->Height - 1 );
	for (int r = 0; r < Device->Height; r += Private->PageSize) {
		if (Private->iRAM) {
			memcpy(Private->iRAM, Device->Framebuffer + r * Device->Width / 2, Private->PageSize * Device->Width / 2 );
			Device->WriteData( Device, Private->iRAM, Private->PageSize * Device->Width / 2 );
		} else	{
			Device->WriteData( Device, Device->Framebuffer + r * Device->Width / 2, Private->PageSize * Device->Width / 2 );
		}	
	}	
#endif	
}

static void SetLayout( struct GDS_Device* Device, struct GDS_Layout *Layout ) { 
    if (Layout->HFlip) {
        Device->WriteCommand( Device, 0x40 + 0x20 );
        Device->WriteCommand( Device, 0xA1 );
    } else {
        Device->WriteCommand( Device, 0x40 + 0x00 );
        Device->WriteCommand( Device, 0xA0 );
    }
	Device->WriteCommand( Device, Layout->VFlip ? 0xC8 : 0xC0 );
    Device->WriteCommand( Device, Layout->Invert ? 0xA7 : 0xA6 );
}	

static void DisplayOn( struct GDS_Device* Device ) { Device->WriteCommand( Device, 0xAF ); }
static void DisplayOff( struct GDS_Device* Device ) { Device->WriteCommand( Device, 0xAE ); }

static void SetContrast( struct GDS_Device* Device, uint8_t Contrast ) {
    Device->WriteCommand( Device, 0x81 );
    Device->WriteCommand( Device, Contrast );
}

static bool Init( struct GDS_Device* Device ) {
	struct PrivateSpace *Private = (struct PrivateSpace*) Device->Private;
	
	// find a page size that is not too small is an integer of height
	Private->PageSize = min(8, PAGE_BLOCK / (Device->Width / 2));
	while (Private->PageSize && Device->Height != (Device->Height / Private->PageSize) * Private->PageSize) Private->PageSize--;
	
#ifdef SHADOW_BUFFER	
	Private->Shadowbuffer = malloc( Device->FramebufferSize );	
	memset(Private->Shadowbuffer, 0xFF, Device->FramebufferSize);
#endif

    // only use iRAM for SPI
    if (Device->IF == GDS_IF_SPI) {
        Private->iRAM = heap_caps_malloc( Private->PageSize * Device->Width / 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA );
    }

	ESP_LOGI(TAG, "SH1122 page %u, iRAM %p", Private->PageSize, Private->iRAM);
			
	// need to be off and disable display RAM
	Device->DisplayOff( Device );
    Device->WriteCommand( Device, 0xA5 );
	
	// Display Offset
    Device->WriteCommand( Device, 0xD3 );
    Device->WriteCommand( Device, 0 );
    
 	// set flip modes
	struct GDS_Layout Layout = { };
	Device->SetLayout( Device, &Layout );
    	
	// set Clocks => check value
    Device->WriteCommand( Device, 0xD5 );
    Device->WriteCommand( Device, ( 0x04 << 4 ) | 0x00 );
	
	// MUX Ratio => fixed
    Device->WriteCommand( Device, 0xA8 );
    Device->WriteCommand( Device, Device->Height - 1);
			
	// no Display Inversion
    Device->WriteCommand( Device, 0xA6 );
	
	// gone with the wind
	Device->WriteCommand( Device, 0xA4 );
	Device->DisplayOn( Device );
	Device->Update( Device );
	
	return true;
}	

static const struct GDS_Device SH1122 = {
	.DisplayOn = DisplayOn, .DisplayOff = DisplayOff, .SetContrast = SetContrast,
	.SetLayout = SetLayout,
	.Update = Update, .Init = Init,
	.Mode = GDS_GRAYSCALE, .Depth = 4,
    .HighNibble = true,
};	

struct GDS_Device* SH1122_Detect(char *Driver, struct GDS_Device* Device) {
	if (!strcasestr(Driver, "SH1122")) return NULL;
		
	if (!Device) Device = calloc(1, sizeof(struct GDS_Device));
	*Device = SH1122;

	return Device;
}