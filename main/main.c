#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"



#include "pngSlide.h"



#define SPI_DMA_CHAN    1





static const char *TAG = "MAIN_APP";




PngSlide_t pngSlideInstance;


static PngSlide_t* pngSlide;








static void SPIFFS_Directory(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(__FUNCTION__,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}






void app_main(void) {




	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 32,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total,&used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	SPIFFS_Directory("/spiffs/");





	// -----------------------------------
    // INITIALIZING PNGSLIDE
    ESP_LOGI(TAG, "-----------------------------");
    ESP_LOGI(TAG, "-----------------------------");
    ESP_LOGI(TAG, "Initializing PNGSLIDE");




    pngSlide = PngSlideStart(0);



    // =====================================================
    // < STACK SIZE IS CRITICAL !!!! >
    // :: WHEN YOU CANNOT GET PROPER MULTITASKING WITHOUT A REASON,
    //    YOU SHOULD CHECK THE SIZE OF STACK 
    //    WHEN I DECREASED 'scale' PARAMETER 16 TO 8, WE GOT
    //    MULTITASKING
	PngSlide_Task_Play_Start(pngSlide, 8, 2, tskNO_AFFINITY);




}
