#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "lcd_com.h"
#include "lcd_lib.h"
#include "fontx.h"
#include "bmpfile.h"
#include "decode_jpeg.h"
#include "decode_png.h"
#include "pngle.h"
#include "ili9341.h"

#define INTERFACE INTERFACE_I2S
#define DRIVER "ILI9341"
#define WAIT vTaskDelay(400)

static const char *TAG = "MAIN";

extern TickType_t DisplayFonts(TFT_t * dev, int width, int height);
extern TickType_t DisplayPNG(TFT_t * dev, char * file, int width, int height);

// Check memory leek
void traceHeap() {
	static int flag = 0;
	static uint32_t _first_free_heap_size = 0;
	if (flag == 0) {
		flag++;
	} else if (flag == 1) {
		_first_free_heap_size = esp_get_free_heap_size();
		flag++;
	} else {
		int _diff_free_heap_size = _first_free_heap_size - esp_get_free_heap_size();
		ESP_LOGI(__FUNCTION__, "_diff_free_heap_size=%d", _diff_free_heap_size);

	}
}

static void listSPIFFS(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(__FUNCTION__,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}

esp_err_t mountSPIFFS(char * path, char * label, int max_files) {
	esp_vfs_spiffs_conf_t conf = {
		.base_path = path,
		.partition_label = label,
		.max_files = max_files,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Mount %s to %s success", path, label);
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	return ret;
}

static void initSPIFFS() {
	// Initialize NVS
	ESP_LOGI(TAG, "Initialize NVS");
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	ESP_LOGI(TAG, "Initializing SPIFFS");
	esp_err_t ret;
	ret = mountSPIFFS("/spiffs", "storage0", 10);	// Fonts
	if (ret != ESP_OK) return;
	listSPIFFS("/spiffs/");

	ret = mountSPIFFS("/icons", "storage1", 10);	// Icons
	if (ret != ESP_OK) return;
	listSPIFFS("/icons/");

	ret = mountSPIFFS("/images", "storage2", 14);	// Images
	if (ret != ESP_OK) return;
	listSPIFFS("/images/");

	// Partitions created and files copied using the commands in /CMakeLists.txt
}

void TFT(void *pvParameters) {	
	TFT_t dev;
	lcd_interface_cfg(&dev, INTERFACE);

	ili9341_lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);

	while(1) {
		traceHeap();
		ESP_LOGI(TAG, "Display functions started");

		DisplayFonts(&dev, CONFIG_WIDTH, CONFIG_HEIGHT);
		WAIT;
		
		char file[32];
		strcpy(file, "/images/Lenna.png");	// Storing image path
		DisplayPNG(&dev, file, CONFIG_WIDTH, CONFIG_HEIGHT);
		WAIT;
	} // end while
}

void app_main() {
	initSPIFFS();
	xTaskCreate(TFT, "TFT", 1024*6, NULL, 2, NULL);
}