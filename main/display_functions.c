#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

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

#define INTERVAL 400
#define WAIT vTaskDelay(INTERVAL)

TickType_t DisplayPNG(TFT_t * dev, char * file, int width, int height) {
	ESP_LOGD(__FUNCTION__, "file=[%s]", file);
	TickType_t startTick, endTick, diffTick;
	startTick = xTaskGetTickCount();

	lcdFillScreen(dev, BLACK);

	int _width = width;
	if (width > 240) _width = 240;
	int _height = height;
	if (height > 320) _height = 320;

	// open PNG file
	FILE* fp = fopen(file, "rb");
	if (fp == NULL) {
		ESP_LOGW(__FUNCTION__, "File not found [%s]", file);
		return 0;
	}

	char buf[1024];
	size_t remain = 0;
	int len;

	pngle_t *pngle = pngle_new(_width, _height);
	ESP_LOGD(__FUNCTION__, "pngle=%p", pngle);
	if (pngle == NULL) {
		fclose(fp);
		return 0;
	}

	pngle_set_init_callback(pngle, png_init);
	pngle_set_draw_callback(pngle, png_draw);
	pngle_set_done_callback(pngle, png_finish);

	double display_gamma = 2.2;
	pngle_set_display_gamma(pngle, display_gamma);


	while (!feof(fp)) {
		if (remain >= sizeof(buf)) {
			ESP_LOGE(__FUNCTION__, "Buffer exceeded");
			while(1) vTaskDelay(1);
		}

		len = fread(buf + remain, 1, sizeof(buf) - remain, fp);
		if (len <= 0) {
			//printf("EOF\n");
			break;
		}

		int fed = pngle_feed(pngle, buf, remain + len);
		ESP_LOGD(__FUNCTION__, "pngle_feed fed=%d", fed);
		if (fed < 0) {
			ESP_LOGE(__FUNCTION__, "ERROR; %s", pngle_error(pngle));
			pngle_destroy(pngle, _width, _height);
			return 0;
		}

		remain = remain + len - fed;
		if (remain > 0) memmove(buf, buf + fed, remain);
	}
	ESP_LOGD(__FUNCTION__, "pngle_feed finish");
	fclose(fp);

	uint16_t pngWidth = width;
	uint16_t offsetX = 0;
	if (width > pngle->imageWidth) {
		pngWidth = pngle->imageWidth;
		offsetX = (width - pngle->imageWidth) / 2;
	}
	ESP_LOGD(__FUNCTION__, "pngWidth=%d offsetX=%d", pngWidth, offsetX);

	uint16_t pngHeight = height;
	uint16_t offsetY = 0;
	if (height > pngle->imageHeight) {
		pngHeight = pngle->imageHeight;
		offsetY = (height - pngle->imageHeight) / 2;
	}
	ESP_LOGD(__FUNCTION__, "pngHeight=%d offsetY=%d", pngHeight, offsetY);
	uint16_t *colors = (uint16_t*)malloc(sizeof(uint16_t) * pngWidth);
	if (colors == NULL) {
		ESP_LOGE(__FUNCTION__, "malloc fail");
		pngle_destroy(pngle, _width, _height);
		return 0;
	}

#if 0
	for(int y = 0; y < pngHeight; y++){
		for(int x = 0;x < pngWidth; x++){
			pixel_png pixel = pngle->pixels[y][x];
			uint16_t color = rgb565_conv(pixel.red, pixel.green, pixel.blue);
			lcdDrawPixel(dev, x+offsetX, y+offsetY, color);
		}
	}
#endif

	for(int y = 0; y < pngHeight; y++){
		for(int x = 0;x < pngWidth; x++){
			//pixel_png pixel = pngle->pixels[y][x];
			//colors[x] = rgb565_conv(pixel.red, pixel.green, pixel.blue);
			colors[x] = pngle->pixels[y][x];
		}
		lcdDrawMultiPixels(dev, offsetX, y+offsetY, pngWidth, colors);
	}
	free(colors);
	pngle_destroy(pngle, _width, _height);

	endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;
	ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%"PRIu32,diffTick*portTICK_PERIOD_MS);
	return diffTick;
}

TickType_t DisplayFonts(TFT_t dev, int width, int height) {
	TickType_t startTick, endTick, diffTick;
	startTick = xTaskGetTickCount();

    // set font file
	FontxFile fx16G[2];
	FontxFile fx24G[2];
	FontxFile fx32G[2];
	InitFontx(fx16G,"/spiffs/ILGH16XB.FNT",""); // 8x16Dot Gothic
	InitFontx(fx24G,"/spiffs/ILGH24XB.FNT",""); // 12x24Dot Gothic
	InitFontx(fx32G,"/spiffs/ILGH32XB.FNT",""); // 16x32Dot Gothic

	FontxFile fx32L[2];
	InitFontx(fx32L,"/spiffs/LATIN32B.FNT",""); // 16x32Dot Latinc

	FontxFile fx16M[2];
	FontxFile fx24M[2];
	FontxFile fx32M[2];
	InitFontx(fx16M,"/spiffs/ILMH16XB.FNT",""); // 8x16Dot Mincyo
	InitFontx(fx24M,"/spiffs/ILMH24XB.FNT",""); // 12x24Dot Mincyo
	InitFontx(fx32M,"/spiffs/ILMH32XB.FNT",""); // 16x32Dot Mincyo

    // Multi Font Test
    uint16_t color;
    uint8_t ascii[40];
    uint16_t margin = 10;
    lcdFillScreen(&dev, BLACK);
    color = WHITE;
    lcdSetFontDirection(&dev, DIRECTION0);
    uint16_t xpos = 0;
    uint16_t ypos = 15;
    int xd = 0;
    int yd = 1;
    if(width < height) {
        lcdSetFontDirection(&dev, DIRECTION90);
        xpos = (width-1)-16;
        ypos = 0;
        xd = 1;
        yd = 0;
    }

	//Line1
    strcpy((char *)ascii, "16Dot Gothic Font");
    lcdDrawString(&dev, fx16G, xpos, ypos, ascii, color);

	//Line2
    xpos = xpos - (24 * xd) - (margin * xd);
    ypos = ypos + (16 * yd) + (margin * yd);
    strcpy((char *)ascii, "24Dot Gothic Font");
    lcdDrawString(&dev, fx24G, xpos, ypos, ascii, color);

	//Line3
    xpos = xpos - (32 * xd) - (margin * xd);
    ypos = ypos + (24 * yd) + (margin * yd);
    if (width >= 240) {
        strcpy((char *)ascii, "32Dot Gothic Font");
        lcdDrawString(&dev, fx32G, xpos, ypos, ascii, color);
        xpos = xpos - (32 * xd) - (margin * xd);;
        ypos = ypos + (32 * yd) + (margin * yd);
    }

	//Line4
    xpos = xpos - (10 * xd) - (margin * xd);
    ypos = ypos + (10 * yd) + (margin * yd);
    strcpy((char *)ascii, "16Dot Mincyo Font");
    lcdDrawString(&dev, fx16M, xpos, ypos, ascii, color);

	//Line5
    xpos = xpos - (24 * xd) - (margin * xd);;
    ypos = ypos + (16 * yd) + (margin * yd);
    strcpy((char *)ascii, "24Dot Mincyo Font");
    lcdDrawString(&dev, fx24M, xpos, ypos, ascii, color);

	//Line6
    if (width >= 240) {
        xpos = xpos - (32 * xd) - (margin * xd);;
        ypos = ypos + (24 * yd) + (margin * yd);
        strcpy((char *)ascii, "32Dot Mincyo Font");
        lcdDrawString(&dev, fx32M, xpos, ypos, ascii, color);
    }
	
    lcdSetFontDirection(&dev, DIRECTION0);

    endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;
	ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%"PRIu32,diffTick*portTICK_PERIOD_MS);
	return diffTick;
}