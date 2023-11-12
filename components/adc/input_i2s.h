/*
 *  (c) Philippe 2020, philippe_44@outlook.com
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 *
 */

#pragma once

#include "adc_sink.h"

struct adc_ctx_s* adc_create(adc_cmd_cb_t cmd_cb, adc_data_cb_t data_cb);
void  		  adc_delete(struct adc_ctx_s *ctx);
void		  adc_abort(struct adc_ctx_s *ctx);
bool		  adc_cmd(struct adc_ctx_s *ctx, adc_event_t event, void *param);
