/* Console example

 This example code is in the Public Domain (or CC0 licensed, at your option.)

 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
 */

#include "platform_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "nvs.h" 
#include "nvs_flash.h"
#include "pthread.h"
#include "platform_esp32.h"
#include "cmd_decl.h"
#include "trace.h"
#include "platform_config.h"
#include "telnet.h" 
#include "tools.h"

#include "messaging.h"

#include "config.h"
static pthread_t thread_console;
static void * console_thread();
void console_start();
static const char * TAG = "console";
extern bool bypass_network_manager;
extern void register_squeezelite();

static EXT_RAM_ATTR QueueHandle_t uart_queue;
static EXT_RAM_ATTR struct {
		uint8_t _buf[512];
		StaticRingbuffer_t _ringbuf;
		RingbufHandle_t handle;
		QueueSetHandle_t queue_set;
} stdin_redir;	

/* Prompt to be printed before each line.
 * This can be customized, made dynamic, etc.
 */
const char* prompt = LOG_COLOR_I "squeezelite-esp32> " LOG_RESET_COLOR;
const char* recovery_prompt = LOG_COLOR_E "recovery-squeezelite-esp32> " LOG_RESET_COLOR;

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"
static esp_err_t run_command(char * line);
#define ADD_TO_JSON(o,t,n) if (t->n) cJSON_AddStringToObject(o,QUOTE(n),t->n);
#define ADD_PARMS_TO_CMD(o,t,n) { cJSON * parms = ParmsToJSON(&t.n->hdr); if(parms) cJSON_AddItemToObject(o,QUOTE(n),parms); }
cJSON * cmdList;
cJSON * values_fn_list;
cJSON * get_cmd_list(){
	cJSON * element;
	cJSON * values=cJSON_CreateObject();
	cJSON * list = cJSON_CreateObject();
	cJSON_AddItemReferenceToObject(list,"commands",cmdList);
	cJSON_AddItemToObject(list,"values",values);
	cJSON_ArrayForEach(element,cmdList){
		cJSON * name = cJSON_GetObjectItem(element,"name");
		cJSON * vals_fn = cJSON_GetObjectItem(values_fn_list,cJSON_GetStringValue(name));
		if(vals_fn!=NULL ){
			parm_values_fn_t *parm_values_fn = (parm_values_fn_t *)strtoul(cJSON_GetStringValue(vals_fn), NULL, 16);;

			if(parm_values_fn){
				cJSON_AddItemToObject(values,cJSON_GetStringValue(name),parm_values_fn());
			}
		}
	}
	return list;
}
void console_set_bool_parameter(cJSON * root,char * nvs_name, struct arg_lit *arg){
    char * p=NULL;
    if(!root) {
        ESP_LOGE(TAG,"Invalid json parameter. Cannot set %s from %s",arg->hdr.longopts?arg->hdr.longopts:arg->hdr.glossary,nvs_name);
        return;
    }
    if ((p = config_alloc_get(NVS_TYPE_STR, nvs_name)) != NULL) {
        cJSON_AddBoolToObject(root,arg->hdr.longopts,strcmp(p,"1") == 0 || strcasecmp(p,"y") == 0);
        FREE_AND_NULL(p);
    }
}
struct arg_end *getParmsEnd(struct arg_hdr * * argtable){
	if(!argtable) return NULL;
	struct arg_hdr * *table = (struct arg_hdr * *)argtable;
	int tabindex = 0;
	while (!(table[tabindex]->flag & ARG_TERMINATOR))
	{
		tabindex++;
	}
	return (struct arg_end *)table[tabindex];
}
cJSON * ParmsToJSON(struct arg_hdr * * argtable){
	if(!argtable) return NULL;
	cJSON * arg_list = cJSON_CreateArray();
	struct arg_hdr * *table = (struct arg_hdr * *)argtable;
	int tabindex = 0;
	while (!(table[tabindex]->flag & ARG_TERMINATOR))
	{
		cJSON * entry = cJSON_CreateObject();
		ADD_TO_JSON(entry,table[tabindex],datatype);
		ADD_TO_JSON(entry,table[tabindex],glossary);
		ADD_TO_JSON(entry,table[tabindex],longopts);
		ADD_TO_JSON(entry,table[tabindex],shortopts);
		cJSON_AddBoolToObject(entry, "checkbox", (table[tabindex]->flag & ARG_HASOPTVALUE)==0 && (table[tabindex]->flag & ARG_HASVALUE)==0 && (table[tabindex]->longopts || table[tabindex]->shortopts) );
		cJSON_AddBoolToObject(entry, "remark", (table[tabindex]->flag & ARG_HASOPTVALUE)==0 && (table[tabindex]->flag & ARG_HASVALUE)==0 && (!table[tabindex]->longopts && !table[tabindex]->shortopts));
		cJSON_AddBoolToObject(entry, "hasvalue", table[tabindex]->flag & ARG_HASVALUE);
		cJSON_AddNumberToObject(entry,"mincount",table[tabindex]->mincount);
		cJSON_AddNumberToObject(entry,"maxcount",table[tabindex]->maxcount);
		cJSON_AddItemToArray(arg_list, entry);
		tabindex++;
	}
	return arg_list;
}

esp_err_t cmd_to_json(const esp_console_cmd_t *cmd){
	return cmd_to_json_with_cb(cmd, NULL);
}

esp_err_t cmd_to_json_with_cb(const esp_console_cmd_t *cmd, parm_values_fn_t parm_values_fn){
	if(!cmdList){
		cmdList=cJSON_CreateArray();
	}
	if(!values_fn_list){
		values_fn_list=cJSON_CreateObject();
	}

    if (cmd->command == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strchr(cmd->command, ' ') != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON * jsoncmd = cJSON_CreateObject();
    ADD_TO_JSON(jsoncmd,cmd,help);
    ADD_TO_JSON(jsoncmd,cmd,hint);
	if(parm_values_fn){
		char addr[11]={0};
		snprintf(addr,sizeof(addr),"%lx",(unsigned long)parm_values_fn);
		cJSON_AddStringToObject(values_fn_list,cmd->command,addr);
	}
	cJSON_AddBoolToObject(jsoncmd,"hascb",parm_values_fn!=NULL);

    if(cmd->argtable){
    	cJSON_AddItemToObject(jsoncmd,"argtable",ParmsToJSON(cmd->argtable));
    }
    if (cmd->hint) {
    	cJSON_AddStringToObject(jsoncmd, "hint", cmd->hint);
    }
    else if (cmd->argtable) {
        /* Generate hint based on cmd->argtable */
        char *buf = NULL;
        size_t buf_size = 0;
        FILE *f = open_memstream(&buf, &buf_size);
        if (f != NULL) {
            arg_print_syntax(f, cmd->argtable, NULL);
            fflush(f);
            fclose(f);
        }
        cJSON_AddStringToObject(jsoncmd, "hint", buf);
        FREE_AND_NULL(buf);
    }
    cJSON_AddStringToObject(jsoncmd, "name", cmd->command);
    char * b=cJSON_Print(jsoncmd);
    if(b){
    	ESP_LOGD(TAG,"Adding command table %s",b);
    	free(b);
    }
    cJSON_AddItemToArray(cmdList, jsoncmd);
    return ESP_OK;
}
int arg_parse_msg(int argc, char **argv, struct arg_hdr ** args){
    int nerrors = arg_parse(argc, argv, (void **)args);

    if (nerrors != 0) {
    	char *buf = NULL;
		size_t buf_size = 0;
		FILE *f = open_memstream(&buf, &buf_size);
		if (f != NULL) {
			arg_print_errors(f, getParmsEnd(args), argv[0]);
			fflush (f);
			cmd_send_messaging(argv[0],MESSAGING_ERROR,"%s", buf);
		}
        fclose(f);
        FREE_AND_NULL(buf);
    }
    return nerrors;
}
void process_autoexec(){
	int i=1;
	char autoexec_name[24]={0};
	char * autoexec_value=NULL;
	uint8_t autoexec_flag=0;

	char * str_flag = config_alloc_get(NVS_TYPE_STR, "autoexec");
	if(!bypass_network_manager){
		ESP_LOGW(TAG, "Processing autoexec commands while network manager active.  Wifi related commands will be ignored.");
	}
	if(is_recovery_running){
		ESP_LOGD(TAG, "Processing autoexec commands in recovery mode.  Squeezelite commands will be ignored.");
	}
	if(str_flag !=NULL ){
		autoexec_flag=atoi(str_flag);
		ESP_LOGI(TAG,"autoexec is set to %s auto-process", autoexec_flag>0?"perform":"skip");
		if(autoexec_flag >= 1) {
			do {
				if (autoexec_flag == 1) {
					snprintf(autoexec_name,sizeof(autoexec_name)-1, "autoexec%u",i++);
				}
				else {
					snprintf(autoexec_name,sizeof(autoexec_name)-1, "autoexec%u_%u",i++, autoexec_flag);
				}
				ESP_LOGD(TAG,"Getting command name %s", autoexec_name);
				autoexec_value= config_alloc_get(NVS_TYPE_STR, autoexec_name);
				if(autoexec_value!=NULL ){
					if(!bypass_network_manager && strstr(autoexec_value, "join ")!=NULL ){
						ESP_LOGW(TAG,"Ignoring wifi join command.");
					}
					else if(is_recovery_running && !strstr(autoexec_value, "squeezelite " ) ){
						ESP_LOGW(TAG,"Ignoring command. ");
					}
					else {
						ESP_LOGI(TAG,"Running command %s = %s", autoexec_name, autoexec_value);
						run_command(autoexec_value);
					}
					ESP_LOGD(TAG,"Freeing memory for command %s name", autoexec_name);
					free(autoexec_value);
				}
				else {
					ESP_LOGD(TAG,"No matching command found for name %s", autoexec_name);
					break;
				}
			} while(1);
		}
		free(str_flag);
	}
	else
	{
		ESP_LOGD(TAG,"No matching command found for name autoexec.");
	}
}

static ssize_t stdin_read(int fd, void* data, size_t size) {
	size_t bytes = -1;
	
	while (1) {
		QueueSetMemberHandle_t activated = xQueueSelectFromSet(stdin_redir.queue_set, portMAX_DELAY);
	
		if (activated == uart_queue) {
			uart_event_t event;
			
			xQueueReceive(uart_queue, &event, 0);
	
			if (event.type == UART_DATA) {
#if defined (CONFIG_ESP_CONSOLE_UART_DEFAULT)
				bytes = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, data, size < event.size ? size : event.size, 0);
				// we have to do our own line ending translation here 
				for (int i = 0; i < bytes; i++) if (((char*)data)[i] == '\r') ((char*)data)[i] = '\n';
				break;
#endif
			}	
		} else if (xRingbufferCanRead(stdin_redir.handle, activated)) {
			char *p = xRingbufferReceiveUpTo(stdin_redir.handle, &bytes, 0, size);
			// we might receive strings, replace null by \n
			for (int i = 0; i < bytes; i++) if (p[i] == '\0' || p[i] == '\r') p[i] = '\n';						
			memcpy(data, p, bytes);
			vRingbufferReturnItem(stdin_redir.handle, p);
			break;
		}
	}	
	
	return bytes;
}

static int stdin_dummy(const char * path, int flags, int mode) {	return 0; }

void initialize_console() {
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
	/* Minicom, screen, idf_monitor send CR when ENTER key is pressed (unused if we redirect stdin) */
	esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
	/* Move the caret to the beginning of the next line on '\n' */
	esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

	/* Configure UART. Note that REF_TICK is used so that the baud rate remains
	 * correct while APB frequency is changing in light sleep mode.
	 */
	const uart_config_t uart_config = { .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE, 
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        .source_clk = UART_SCLK_REF_TICK,
#else
        .source_clk = UART_SCLK_XTAL,
#endif			
	};
	ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));

	/* Install UART driver for interrupt-driven reads and writes */
	ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 3, &uart_queue, 0));
	
	/* Tell VFS to use UART driver */
	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    const usb_serial_jtag_driver_config_t jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&jtag_config);

    esp_vfs_usb_serial_jtag_use_driver();
#else
#error Unsupported console type
#endif
	/* TODO: TELNET AND REDIRECT NOT WORKING ON S3/USB_JTAG CONFIGURATION*/
	/* re-direct stdin to our own driver so we can gather data from various sources */
	stdin_redir.queue_set = xQueueCreateSet(2);
	stdin_redir.handle = xRingbufferCreateStatic(sizeof(stdin_redir._buf), RINGBUF_TYPE_BYTEBUF, stdin_redir._buf, &stdin_redir._ringbuf);
	xRingbufferAddToQueueSetRead(stdin_redir.handle, stdin_redir.queue_set);
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT)
	xQueueAddToSet(uart_queue, stdin_redir.queue_set);
#endif
	esp_vfs_t vfs = { };
	vfs.flags = ESP_VFS_FLAG_DEFAULT;
	vfs.open = stdin_dummy;
	vfs.read = stdin_read;

	ESP_ERROR_CHECK(esp_vfs_register("/dev/redirect", &vfs, NULL));
    freopen("/dev/redirect", "r", stdin);

	/* Disable buffering on stdin */
	setvbuf(stdin, NULL, _IONBF, 0);
	/* Initialize the console */
	esp_console_config_t console_config = { .max_cmdline_args = 28,
			.max_cmdline_length = 600,
#if CONFIG_LOG_COLORS
			.hint_color = atoi(LOG_COLOR_CYAN)
#endif
			};
	ESP_ERROR_CHECK(esp_console_init(&console_config));

	/* Configure linenoise line completion library */
	/* Enable multiline editing. If not set, long commands will scroll within
	 * single line.
	 */
	linenoiseSetMultiLine(1);

	/* Tell linenoise where to get command completions and hints */
	linenoiseSetCompletionCallback(&esp_console_get_completion);
	linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

	/* Set command history size */
	linenoiseHistorySetMaxLen(100);

	/* Load command history from filesystem */
	//linenoiseHistoryLoad(HISTORY_PATH);
}

bool console_push(const char *data, size_t size) {
	return xRingbufferSend(stdin_redir.handle, data, size, pdMS_TO_TICKS(100)) == pdPASS;
}	

void console_start() {
	/* we always run console b/c telnet sends commands to stdin */
	initialize_console();

	/* Register commands */
	MEMTRACE_PRINT_DELTA_MESSAGE("Registering help command");
	esp_console_register_help_command();
	MEMTRACE_PRINT_DELTA_MESSAGE("Registering system commands");
	register_system();
	MEMTRACE_PRINT_DELTA_MESSAGE("Registering config commands");
	register_config_cmd();
	MEMTRACE_PRINT_DELTA_MESSAGE("Registering nvs commands");
	register_nvs();
	MEMTRACE_PRINT_DELTA_MESSAGE("Registering wifi commands");
	register_wifi();

	if(!is_recovery_running){
		MEMTRACE_PRINT_DELTA_MESSAGE("Registering squeezelite commands");
		register_squeezelite();
	}
	else {
		MEMTRACE_PRINT_DELTA_MESSAGE("Registering recovery commands");
		register_ota_cmd();
	}
	MEMTRACE_PRINT_DELTA_MESSAGE("Registering i2c commands");
	register_i2ctools();
	
	printf("\n");
	if(is_recovery_running){
		printf("****************************************************************\n"
		"RECOVERY APPLICATION\n"
		"This mode is used to flash Squeezelite into the OTA partition\n"
		"****\n\n");
	}
	printf("Type 'help' to get the list of commands.\n"
	"Use UP/DOWN arrows to navigate through command history.\n"
	"Press TAB when typing command name to auto-complete.\n"
	"\n");
	if(!is_recovery_running){
		printf("To automatically execute lines at startup:\n"
				"\tSet NVS variable autoexec (U8) = [1~N] to enable command sets, 0 to disable automatic execution.\n"
				"\tSet NVS variable autoexec[1~9][_N] (string)to a command that should be executed automatically\n");
	}
	printf("\n\n");

	/* Figure out if the terminal supports escape sequences */
	int probe_status = linenoiseProbe();
	if (probe_status) { /* zero indicates success */
		printf("\n****************************\n"
				"Your terminal application does not support escape sequences.\n"
				"Line editing and history features are disabled.\n"
				"On Windows, try using Putty instead.\n"
				"****************************\n");
		linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
		/* Since the terminal doesn't support escape sequences,
		 * don't use color codes in the prompt.
		 */
		if(is_recovery_running){
			recovery_prompt=  "recovery-squeezelite-esp32>";
		}
		prompt = "squeezelite-esp32> ";
#endif //CONFIG_LOG_COLORS
	}
	esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
	cfg.thread_name= "console";
	cfg.inherit_cfg = true;
	cfg.stack_size = 4*1024;
	if(is_recovery_running){
		prompt = recovery_prompt;
	}
		MEMTRACE_PRINT_DELTA_MESSAGE("Creating console thread with stack size of 4096 bytes");
	esp_pthread_set_cfg(&cfg);
	pthread_create(&thread_console, NULL, console_thread, NULL);
	MEMTRACE_PRINT_DELTA_MESSAGE("Console thread created");

}

static esp_err_t run_command(char * line){
	/* Try to run the command */
	int ret;
	esp_err_t err = esp_console_run(line, &ret);

	if (err == ESP_ERR_NOT_FOUND) {
		ESP_LOGE(TAG,"Unrecognized command: %s", line);
	} else if (err == ESP_ERR_INVALID_ARG) {
		// command was empty
	} else if (err != ESP_OK && ret != ESP_OK) {
		ESP_LOGW(TAG,"Command returned non-zero error code: 0x%x (%s)", ret,
		esp_err_to_name(err));
	} else if (err == ESP_OK && ret != ESP_OK) {
		ESP_LOGW(TAG,"Command returned in error");
		err = ESP_FAIL;
	} else if (err != ESP_OK) {
		ESP_LOGE(TAG,"Internal error: %s", esp_err_to_name(err));
	}
	return err;
}

static void * console_thread() {
	if(!is_recovery_running){
		MEMTRACE_PRINT_DELTA_MESSAGE("Running autoexec");
		process_autoexec();
		MEMTRACE_PRINT_DELTA_MESSAGE("Autoexec done");
	}
	/* Main loop */
	while (1) {
		/* Get a line using linenoise.
		 * The line is returned when ENTER is pressed.
		 */
		char* line = linenoise(prompt);
		if (line == NULL) { /* Ignore empty lines */
			continue;
		}
		/* Add the command to the history */
		linenoiseHistoryAdd(line);

		/* Save command history to filesystem */
		linenoiseHistorySave(HISTORY_PATH);
		printf("\n");
		run_command(line);
		/* linenoise allocates line buffer on the heap, so need to free it */
		linenoiseFree(line);
		taskYIELD();
	}
	return NULL;
}

