/* 
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
 *      Ralph Irving 2015-2017, ralph_irving@hotmail.com
 *		Philippe_44	 2020, philippe_44@outloook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "squeezelite.h"

#define VISUEXPORT_SIZE	512

EXT_BSS struct visu_export_s visu_export;
static struct visu_export_s *visu = &visu_export;

static log_level loglevel = lINFO;

void output_visu_export(void *frames, frames_t out_frames, u32_t rate, bool silence, u32_t gain) {
	
	// no data to process
	if (silence) {
		visu->running = false;
		return;
	}	
	
	// do not block, try to stuff data but wait for consumer to have used them
	if (!pthread_mutex_trylock(&visu->mutex)) {
		// don't mix sample rates
		if (visu->rate != rate) visu->level = 0;
		
		// stuff buffer up and wait for consumer to read it (should reset level)
		if (visu->level < visu->size) {
			u32_t space = min(visu->size - visu->level, out_frames) * BYTES_PER_FRAME;
			memcpy(visu->buffer + visu->level, frames, space);
			
			visu->level += space / BYTES_PER_FRAME;
			visu->running = true;
			visu->rate = rate ? rate : 44100;
			visu->gain = gain;
		}
		
		// mutex must be released 		
		pthread_mutex_unlock(&visu->mutex);
	} 
}

void output_visu_close(void) {
	pthread_mutex_lock(&visu->mutex);
	visu->running = false;
	free(visu->buffer);
	pthread_mutex_unlock(&visu->mutex);
}

void output_visu_init(log_level level) {
	loglevel = level;
	pthread_mutex_init(&visu->mutex, NULL);
	visu->size = VISUEXPORT_SIZE;
	visu->running = false;
	visu->rate = 44100;
	visu->buffer = malloc(VISUEXPORT_SIZE * BYTES_PER_FRAME);
	LOG_INFO("Initialize VISUEXPORT %u %u bits samples", VISUEXPORT_SIZE, BYTES_PER_FRAME * 4);
}

