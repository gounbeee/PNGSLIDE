
#include "esp_spiffs.h"
#include "esp_log.h"

#include "pngSlide.h"

//#include "uart_slave_lcd.h"

#define TAG "spiffs"



// -----------------------------------------------------------------
// MEMO

// < PASSING 2D ARRAY TO FUNCTION >
// https://www.geeksforgeeks.org/pass-2d-array-parameter-c/

// < SETTING MAXIMUM PRIORITY >
// configMAX_PRIORITIES-1
// https://www.freertos.org/RTOS-task-priority.html


// < ABOUT STACK SIZE >
// https://www.freertos.org/FAQMem.html#StackSize


// < ABOUT EXECUTION SPEED >
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/speed.html


// < E (36103) vfs_fat: open: no free file descriptors >
// https://rntlab.com/question/vfs_fat-open-no-free-file-descriptors/
// : I think that error happens because you try to load too many files at the same time.
//   How are you serving your files? can you show me the snippet of code that does that?


// -----------------------------------------------------------------

PngSlide_t pngSlideInstance;

int gUartDt;
int gUartDtLast;




int protectSize( int length , char * channel) {

	if( strcmp(channel, "WIDTH") == 0) {
		
		if( length > CONFIG_WIDTH ) return CONFIG_WIDTH;
		else return length;

	} else if ( strcmp(channel, "HEIGHT") == 0 ) {

		if( length > CONFIG_HEIGHT) return CONFIG_HEIGHT;
		else return length;

	} else {

		ESP_LOGE(__FUNCTION__, "  @@ IMAGE SIZE PROTECTION ERROR !!!!");
		return -1;

	}

}




pngle_t* PngSlidePlayCurrentFrame(TFT_t * dev,  pngle_t* pngle, int canvWidth, int canvHeight) {

	// AT FIRST, WE NEED TO 'RESET' pngle_t STRUCT IN pngleAnim STRUCT
	// MEMORY DE-ALLOCATION
	pngle_reset(pngle);


	return playFrame(dev, pngle, canvWidth, canvHeight);

}





// < PASSING 2D ARRAY AS A PARAMETER FOR FUNCTION >
// https://stackoverflow.com/questions/28469244/passing-a-2-dimensional-character-array-to-a-function
//
//pngleAnim_t *engineInit(TFT_t * dev, char **fileList, int canvWidth, int canvHeight, int startInd) {
void engineInit(PngSlide_t* pngSlide) {


	// IMAGE INDEX
	int _img_current = pngSlide->hPngAnim.currentFrame;




	// TODO :: BELOW IS FOR DEBUG !
	// OPEN FIRST AND SECOND IMAGE
	// STORING TO GLOBAL ARRAY
    // for(int i = 0; i < MAXFILENUM; i++) {
    // 	printf("i %i | %s\n", i, pngSlide->hPngAnim.fileList[i]);   
   	// 	//printf("CHARACTER :: %c\n", *(*(fileList+0)+2) );
    // }





    // LOADING 2 IMAGE FILES FOR DOUBLE BUFFERING
	FILE* imageCurrent 		= fopen(pngSlide->hPngAnim.fileList[_img_current], "rb");
	//FILE* imageNext			= fopen(pngSlide->hPngAnim.fileList[_img_next],    "rb");



	if (imageCurrent == NULL) {
		ESP_LOGW(__FUNCTION__, "File not found [%s]", pngSlide->hPngAnim.fileList[_img_current]);
		//return 0;
	} 

	//if(imageNext == NULL) {
	//	ESP_LOGW(__FUNCTION__, "File not found [%s]", pngSlide->hPngAnim.fileList[_img_next]);
	//	//return 0;
	//}

 
    // IMAGE SIZE PROTECTION
    // TODO :: TRY TO USE GETTER FUNCTION FOR SIZE
	int _width  = protectSize(CONFIG_WIDTH,  "WIDTH");
	int _height = protectSize(CONFIG_HEIGHT, "HEIGHT");
	
	printf("-- PROTECTED Canvas Width  :: %d \n", _width);
	printf("-- PROTECTED Canvas Height :: %d \n", _height);


	// < FILLING UP BUFFER FOR CURRENT IMAGE >

	// BUFFER SIZE :: 1KB
	char buf_a[1024];

	// REMAINED BUFFER
	// THIS SHOULD BE 0 AFTER ALL DATA WAS TRANSFERRED
	size_t remain_a = 0;
	
	// LENGTH OF READ DATA FROM FILE
	int len_a = 0;






	// PNG STRUCT HANDLE
	// MEMORY ALLOCATION IS DONE
	if(pngSlide->hPngAnim.pngle == NULL) {
		pngSlide->hPngAnim.pngle = pngle_new(_width, _height);	
	}
	



	// CALLBACK FUNCTION SETTED
	pngle_set_init_callback(pngSlide->hPngAnim.pngle, png_init);
	pngle_set_draw_callback(pngSlide->hPngAnim.pngle, png_draw);
	pngle_set_done_callback(pngSlide->hPngAnim.pngle, png_finish);




	// COLOR SETTINGS (GAMMA)
	double display_gamma = 2.2;
	pngle_set_display_gamma(pngSlide->hPngAnim.pngle, display_gamma);











	// < BUFFERING >
	// PREPARING pngSlide->hPngAnim.pngle STRUCT WITH BUFFER
	while (!feof(imageCurrent)) {


		if ( remain_a >= sizeof(buf_a) ) {
			ESP_LOGE(__FUNCTION__, "Buffer exceeded");
			// FORCEFULLY TRAPPED !
			while(1) vTaskDelay(1);
		}




		len_a = fread(buf_a + remain_a, 1, sizeof(buf_a) - remain_a, imageCurrent);

		//printf("-- READING BUFFER :: FILE NUMBER : %d  READ BUFFER :  %d \n", _img_current, len_a);

		if (len_a <= 0) {
			//printf("EOF\n");
			break;
		}

		int fed = pngle_feed(pngSlide->hPngAnim.pngle, buf_a, remain_a + len_a);

		if (fed < 0) {
			ESP_LOGE(__FUNCTION__, "ERROR; %s", pngle_error(pngSlide->hPngAnim.pngle));
			while(1) vTaskDelay(1);
		}

		remain_a = remain_a + len_a - fed;
		if (remain_a > 0) memmove(buf_a, buf_a + fed, remain_a);


		//printf("-- CLEARING BUFFER :: FILE NUMBER : %d  REMAINED BUFFER :  %d \n", _img_current, remain_a);
	
	}




	// TRANSFERRING POINTER FOR SINGLE IMAGE TO ANIMATION STRUCT
	pngSlide->hPngAnim.currentFrame = _img_current;
	pngSlide->hPngAnim.fileCurrent = imageCurrent;


	imageCurrent 	= NULL;


	
	


}




// < PAINT 1 IMAGE >
pngle_t *playFrame(TFT_t * dev,  pngle_t* pngle, int canvWidth, int canvHeight) {
	
	TickType_t startTick, endTick, diffTick;
	startTick = xTaskGetTickCount();


	// ---------------------------------------------------------------------
	// CENTERING IMAGE
	uint16_t pngWidth = canvWidth;
	uint16_t offsetX = 0;
	if (canvWidth > pngle->imageWidth) {
		pngWidth = pngle->imageWidth;
		offsetX = (canvWidth - pngle->imageWidth) / 2;
	}
	ESP_LOGD(__FUNCTION__, "pngWidth=%d offsetX=%d", pngWidth, offsetX);

	uint16_t pngHeight = canvHeight;
	uint16_t offsetY = 0;
	
	if (canvHeight > pngle->imageHeight) {
		pngHeight = pngle->imageHeight;
		offsetY = (canvHeight - pngle->imageHeight) / 2;
	}
	
	ESP_LOGD(__FUNCTION__, "pngHeight=%d offsetY=%d", pngHeight, offsetY);
	

	// ---------------------------------------------------------------------
	// ALLOCATING MEMORY FOR PIXEL COLOR
	uint16_t* colors = (uint16_t*)malloc(sizeof(uint16_t) * pngWidth);


	// ---------------------------------------------------------------------
	// DRAWING PIXELS
	
	// for(int y = 0; y < pngHeight; y++){
	// 	for(int x = 0;x < pngWidth; x++){
	// 		pixel_png pixel = pngle->pixels[y][x];
	// 		uint16_t color = rgb565_conv(pixel.red, pixel.green, pixel.blue);
	// 		lcdDrawPixel(dev, x+offsetX, y+offsetY, color);
	// 	}
	// }



	for(int y = 0; y < pngHeight; y++) {

		for(int x = 0;x < pngWidth; x++) {

			//pixel_png pixel = pngle->pixels[y][x];
			//colors[x] = rgb565_conv(pixel.red, pixel.green, pixel.blue);
			colors[x] = pngle->pixels[y][x];

		}

		lcdDrawMultiPixels(dev, offsetX, y+offsetY, pngWidth, colors);
		//vTaskDelay(1);

	}
	

	vTaskDelay(FRAMETIME / portTICK_PERIOD_MS);


	// ---------------------------------------------------------------------
	// CLEARING COLORS WE USED FOR THIS FRAME
	//printf("-- CLEARING colors IN THIS FRAME\n");
	free(colors);
	colors = NULL;


	endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;

	ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%d", diffTick*portTICK_PERIOD_MS);


	return pngle;

}





// < FILLING UP 1 IMAGE WITH BUFFER >
//
pngle_t* readyNewBuffer(FILE* file, pngle_t* target) {


	// -----------------------------------------------------------------
	// < NEW BUFFERING >

	// < FILLING UP BUFFER FOR CURRENT IMAGE >

	// BUFFER SIZE :: 1KB
	char buf[1024];

	// REMAINED BUFFER
	// THIS SHOULD BE 0 AFTER ALL DATA WAS TRANSFERRED
	size_t remain = 0;
	
	// LENGTH OF READ DATA FROM FILE
	int len = 0;



	// < BUFFERING NEXT >
	// PREPARING pngle STRUCT WITH BUFFER
	while (!feof(file)) {

		if ( remain >= sizeof(buf) ) {
			ESP_LOGE(__FUNCTION__, "Buffer exceeded");
			while(1) vTaskDelay(1);
		}

		len = fread(buf + remain, 1, sizeof(buf) - remain, file);

		

		//vTaskDelay(10);
		//printf("-- A :: (READING NEXT) READING BUFFER :: FILE NUMBER : %d  READED BUFFER :  %d \n", pngleAnim->currentFrame, len);

		if (len <= 0) {
			//printf("EOF\n");
			break;
		}


		int fed = pngle_feed(target, buf, remain + len);

		if (fed < 0) {
			ESP_LOGE(__FUNCTION__, "ERROR; %s", pngle_error(target));
			while(1) vTaskDelay(1);
		}

		remain = remain + len - fed;
		if (remain > 0) memmove(buf, buf + fed, remain);


		//printf("-- A :: (READING NEXT) CLEARING BUFFER :: FILE NUMBER : %d  REMAINED BUFFER :  %d \n", pngleAnim->currentFrame, remain);


	}

	return target;

}



FILE* getNextImage(pngleAnim_t* hPngAnim) {

	int _img_next    = hPngAnim->currentFrame + 1;

	return fopen(hPngAnim->fileList[_img_next],    "rb");

}




int readyNextImgFile(pngleAnim_t* pngleAnim, int nextFrameNum) {

	// < PREPARING >
	// GETTING CURRENT FRAME NUMBER FROM pngleAnim STRUCT

	// ADDING FRAME NUMBER TO NEXT
	// **** DO NOT APPLY LIKE BELOWS !!!!  
	//      int newFrameNum = pngleAnim->currentFrame;  ****




	// CLOSE CURRENT FILE

	printf("CLOSING CURRENT FILE ....     \r\n");
	if(pngleAnim->fileCurrent != NULL) {
		printf("READY FOR NEXT IMAGE FILE :: pngleAnim->fileCurrent  IS NOT NULL, SO WE fclose IT!\n");
		fclose(pngleAnim->fileCurrent);
		printf("CLOSING CURRENT FILE ....     DONE\r\n");
	}

	if(pngleAnim->fileCurrent == NULL ) {
		printf("pngleAnim->fileCurrent  ::  ERROR OCCURED FOR INIALIZING PNGLEANIM !\n");
		while(1) {
			vTaskDelay(1);
		}
	}


	// --------------------------------------------
	// GETTING NEXT FILE POINTER
	//FILE* nextFile = getNextImage(pngleAnim);

	FILE* nextFile = fopen(pngleAnim->fileList[nextFrameNum],    "rb");


	// --------------------------------------------
	// CHECK FOR 'END OF IMAGE LIST'
	//
	// ****  THIS ACTUALLY MAKES 'LOOP'  ****
	//
	if (nextFile == NULL) {
		ESP_LOGW(__FUNCTION__, "File not found !!!!");
		return -1;
	} 


	// --------------------------------------------
	// NORMALLY PLAY THE FILE

	// OVERWRITTING NEXT FRAME TO CURRENT FRAME
	pngleAnim->fileCurrent 	= nextFile;


	nextFile = NULL;


	// ----------------------------
	// < FOR DEBUG >
	// for(int i = 0; i < 7; i++) {
 	// 	   printf("-- ReadyNextImgFile() FUNCTION ::  i   %i | %s\n", i, pngleAnim->fileList[i]);   
 	// }
	
	// printf("-- ReadyNextImgFile() FUNCTION ::  i   %i | %s\n", 0, pngleAnim->fileList[0]);   
	// printf("-- @@ ReadyNextImgFile() :: pngleAnim->fileList[newFrameNum] ::  i   %i | %s\n", newFrameNum, pngleAnim->fileList[newFrameNum]);   

	// ----------------------------



	// < PREPARING NEXT FILE >
	// FOR THIS WE NEED TO LOAD 'NEW' IMAGE FILE
	// FILE* imageNewNext = fopen(pngleAnim->fileList[newFrameNum], "rb");




	// if (imageNewNext == NULL) {
	// 	ESP_LOGW(__FUNCTION__, "File not found !!!!");
	// 	return -1;
	// } 


	// // APPLYING NEW IMAGE FILE TO pngleAnim STRUCT HANDLE
	// pngleAnim->fileNext = imageNewNext;

	// imageNewNext = NULL;


	return 1;

}	













// < CALCULATING LENGTH OF ARRAY >
// https://www.interviewsansar.com/calculate-length-of-array-in-c/
int calcSizeCharArray(const char** arr) {
	return sizeof(arr) / sizeof(char);
}



int getSeperatorCount(char* inputChr, const char* seperator) {

	int seperatorHitCnt = 0;

	// GETTING FIRST LETTER 
	char* letterNow = *(&inputChr);

	// ------------------------
	// < COUNTING FIRST >
	// GETTING HIT COUNT FOR EVERY LETTERS...
	while( *letterNow != '\0') {
		if(*letterNow == *seperator) {
			seperatorHitCnt++;
		}
		// FOR LOOP
		letterNow++;
	}

	return seperatorHitCnt;

}



// < SPLIT CHAR WITH SEARCHING LETTER >
// https://stackoverflow.com/questions/32858256/split-a-stringa-word-to-letters-in-c
void getSplitIndex(int* output, int outputSize, char* inputChr, const char* seperator) {

	// ------------------------
	// < CREATING RESULT DATA >
	// RESET CURSOR AND COUNTER
	char* letterNow = *(&inputChr);
	int seperatorHitInd = 0;

	// CREATING FINAL RESULT ARRAY
	// { 3, 5 } --> THERE IS SEPERATOR LETTER AT 3th 5th LOCATION

	int locationNow = 0;

	while(*letterNow != '\0') {

		if(*letterNow == *seperator) {
			// STORING CURRENT LETTER-LOCATION
			output[seperatorHitInd] = locationNow;

			seperatorHitInd++;				// FOR NEXT CURSOR NUMBER OF RESULT
		}
		locationNow++;						// FOR LOCATION OF SEARCHED LETTER
		letterNow++;						// FOR LETTER-LOOP
	}

}




int getSplittedWordByIndex(char* inputChr, const char* seperator, int seperatorCnt) {

	// //INITIALIZING OUTPUT CHAR HERE
	// for(int i=0; i<seperatorCnt; i++) {
	// 	char* tar = output + i;
	// 	tar = "-";
	// 	output[i] = *tar;
	// }

	char tempResult[50];

	int curCursorInd = 0;
	char* cursorLetter = *(&inputChr);

	// SEARCHING EVERY LETTERS
	while(*cursorLetter != '\0') {

		// IF DOES NOT MATCH, STORE IT
		if(*cursorLetter != *seperator) {

			// STORING IT
			tempResult[curCursorInd] = *cursorLetter;

			curCursorInd++;
		} else {
			break;
		}

		cursorLetter++;
	}

	return atoi(tempResult);

}


// < LISTING FILE LIST FROM SDCARD >
// https://esp32.com/viewtopic.php?t=22759
int getFileListGrpIndex(const char * dirpath){

	int groupInd = -1;
	int fileCount = 0;

    struct dirent *ep;     
    DIR *dp = opendir(dirpath);

    if (dp != NULL){
        while ((ep = readdir(dp)) != NULL) {
            ESP_LOGI(TAG_PngSlide, "Found %s.", ep->d_name);
 			
			groupInd = getImageGroupIndexFromFile(ep->d_name, "_");
			printf("  ==== groupInd IS ::   %d\r\n", groupInd);

            fileCount++;
        }

        printf("fileCount IS ::  %d\r\n", fileCount);

        (void) closedir (dp);

    } else {
        ESP_LOGE(TAG_PngSlide, "Failed to open directory \'%s\'!", dirpath);
    }

    return groupInd;

}



int	getImageGroupIndexFromFile(char* _fileName, char* _seperator) {

	// < GETTING SEPERATOR INDEX TEST >

	char* fileName = _fileName;
	char* seperator = _seperator;
	int seperatorCnt = getSeperatorCount(fileName, seperator);
	int collected[seperatorCnt];

	getSplitIndex(collected, seperatorCnt, fileName, seperator);
	
	//printf("test SPLIT INDEX ::  %d\r\n", collected[0]);

	// WE WANT TO EXTRACT "abc" FROM ABOVE


	// < PASSING 'UNINITIALIZED' PARAMETER TO FUNCTION >
	// https://stackoverflow.com/questions/62649733/why-it-is-ok-to-pass-an-uninitialized-variable-address-to-pointer-paramter-of-a

	// It's only a problem to read an uninitialized variable. 
	// Taking its address is fine since a variable's address is 
	// fixed for its lifetime.
	// In your first example you have an uninitialized variable 
	// of type int. You take its address (which is valid as above) 
	// and pass it to a function which subsequently dereferences 
	// that address to write the value of a.
	// In the second example, you have an uninitialized variable of 
	// type int *, but then you pass its value to the function. This 
	// is not valid because the value of the variable is indeterminate.
	
	// WE CAN DECLARE THE VARIABLE AND PASS TO FUNCTION WHICH IS ]
	// NOT A POINTER !
	int result; 				
	result = getSplittedWordByIndex(fileName, seperator, seperatorCnt);


	return result;
}




int getFileListCount(char **fList, int sceneNum) {


	const char* fileLocation = "/spiffs/";

	// -----------------------------------------------------------------------------
	// NOW ALL SLIDE PICTURES ARE IN THE FOLDER "ALL_SLIDES"
	// AND WE GOT CSV FILE NAMED "ALL_SLIDES_PAGES.csv" WHICH INCLUDES THE COUNT OF 
	// PICUTRES OF INDIVIDUAL SCENE

	char line[MAX_CHAR_OF_INDEX];


	// LOADING FILE
	char* csvFileName = PAGE_CSV_FILENAME;
	char* csvPath;
	csvPath = malloc(sizeof(char) * MAXFILENAMELENGTH);
	sprintf(csvPath, "%s%s", fileLocation, csvFileName);

	//printf("csvPath IS   ::    %s \n", csvPath);

	FILE* csvFile = fopen(csvPath, "r");

	int searchedFileCount = 0;

	if(csvFile != NULL) {
		// FOR EVERY LINES OF CSV FILE...

		int newCount = 1;
		while(fgets(line, MAX_CHAR_OF_INDEX, csvFile)) {

			// IF THERE IS MATCHED LINE NUMBER...
			if(newCount == sceneNum) {
				// GRAB THAT NUMBER
				searchedFileCount = atoi(line);
				break;
			} 

			// NEXT SEARCHING
			newCount++;
		}

	}

	// IF searchedFileCount = 0, WE NEED TO PLAY DEMO SCENE(COARAMAUSE LOGO ANIMATION)
	// THAT ANIMATION IS CURRENTLY INCLUDES 5 IMAGES SO
	// searchedFileCount SHOULD BE 5 FORCEFULLY
	if(searchedFileCount == 0) searchedFileCount = 5;


	fclose(csvFile);
	free(csvPath);
	csvFile = NULL;

	return searchedFileCount;
}






void makeFileNameList(char** target, int groupIndex) {


	const char* fileLocation = "/spiffs/";

	// -----------------------------------------------------------------------------
	// NOW ALL SLIDE PICTURES ARE IN THE FOLDER "ALL_SLIDES"
	// AND WE GOT CSV FILE NAMED "ALL_SLIDES_PAGES.csv" WHICH INCLUDES THE COUNT OF 
	// PICUTRES OF INDIVIDUAL SCENE

	char line[MAX_CHAR_OF_INDEX];


	// LOADING FILE
	char* csvFileName = PAGE_CSV_FILENAME;
	char* csvPath;
	csvPath = malloc(sizeof(char) * MAXFILENAMELENGTH);
	sprintf(csvPath, "%s%s", fileLocation, csvFileName);

	//printf("csvPath IS   ::    %s \n", csvPath);

	FILE* csvFile = fopen(csvPath, "r");

	int searchedFileCount = 0;
	int accumulatedStartIndex = 0;


	if(csvFile != NULL) {
		// FOR EVERY LINES OF CSV FILE...

		int newCount = 1;
		while(fgets(line, MAX_CHAR_OF_INDEX, csvFile)) {

			// IF THERE IS MATCHED LINE NUMBER...
			if(newCount == groupIndex) {
				// GRAB THAT NUMBER
				searchedFileCount = atoi(line);
				break;
			} 

			// STARTING INDEX
			accumulatedStartIndex += atoi(line);

			// NEXT SEARCHING
			newCount++;
		}


		printf("TEST PRINT ::  accumulatedStartIndex  					::    %d \n", accumulatedStartIndex);
		printf("TEST PRINT ::  searchedFileCount      					::    %d \n", searchedFileCount);


		
		char accumulatedStartIndexChar[30];
		size_t accumulatedStartIndexCharLetters = 0;
		char formattedStartIndex[MAX_CHAR_OF_INDEX];


		// ---------------------------------------------------------------------
		// accumulatedStartIndex SHOULD BE 1 ~ 99999
		// accumulatedStartIndexChar WILL BE 1 WHEN accumulatedStartIndex IS 9
		// accumulatedStartIndexChar WILL BE 2 WHEN accumulatedStartIndex IS 10
		//
		// FINALLY, WE WANT 00010 WHEN THE INDEX NUMBER WAS 10
		//
		//
		// FIRST, WE CONVERT INDEX NUMBER TO CHAR
		// + WE NEED TO PLUS 1 CAUSE FILE INDEX IS 1 BASED
		accumulatedStartIndex += 1;
		accumulatedStartIndex += 5;
		sprintf(accumulatedStartIndexChar, "%d", accumulatedStartIndex);

		// SECOND, WE COUNT THE LETTERS OF STARTING INDEX CHAR
		accumulatedStartIndexCharLetters = strlen(accumulatedStartIndexChar);
		int neededZeroCnt = MAX_CHAR_OF_INDEX - accumulatedStartIndexCharLetters;
		printf("TEST PRINT ::  accumulatedStartIndexCharLetters  		::    %d \n", accumulatedStartIndexCharLetters);
		printf("TEST PRINT ::  neededZeroCnt  							::    %d \n", neededZeroCnt);


		// THIRD, FORMATTING INDEX NUMBER
		for(int i=0; i<neededZeroCnt; i++) {
			formattedStartIndex[i] = '0';
		}

		for(int i=0; i<accumulatedStartIndexCharLetters; i++) {
			formattedStartIndex[i + neededZeroCnt] = accumulatedStartIndexChar[i];
		}
		formattedStartIndex[accumulatedStartIndexCharLetters + neededZeroCnt] = '\0';


		printf("TEST PRINT ::  formattedStartIndex       				::    %s \n", formattedStartIndex);


		// ----------------------------------------------------
		// < LOGO ANIMATION >
		// RESET IF searchedFileCount IS 0
		// IN THIS CASE WE NEED TO PLAY COARAMAUSE LOGO SCENE
		if(searchedFileCount == 0) {

			neededZeroCnt = 4;
			accumulatedStartIndex = 1;

			searchedFileCount = 5;

			printf("$$$$  -----------------------------------------  \n");
			printf("$$$$  RESETTED BECAUSE   searchedFileCount == 0  \n");
			printf("TEST PRINT :: FINAL :: RESETTED :: searchedFileCount      			::    %d \n", searchedFileCount);
			printf("TEST PRINT :: FINAL :: RESETTED :: accumulatedStartIndex      		::    %d \n", accumulatedStartIndex);



			// CREATING FILE LIST
			for(int i=1; i<=searchedFileCount; i++) {

				if(target[i-1] != NULL) {
					// CREATING SINGLE IMAGE FILE PATH (ONE LINE)
					char* singlePath;
					singlePath = malloc(sizeof(char) * MAXFILENAMELENGTH);


					// --------------------------------
					// CREATING ZERO LETTERS
					char zeroLetters[ neededZeroCnt ];

					for(int j=0; j<neededZeroCnt; j++) {
						zeroLetters[j] = '0';
					}
					zeroLetters[neededZeroCnt] = '\0';

					// CALCULATING FINAL INDEX
					int finalIndex = accumulatedStartIndex + (i-1);

					// FORMATTING PATH OF STRING FOR SINGLE FILE
					sprintf(singlePath, "%spg/pg%s%d.png", fileLocation, zeroLetters, finalIndex);


					printf("TEST PRINT :: FINAL :: RESETTED :: i   =    %d     ::   zeroLetters    =    %s \n", i , zeroLetters);
					printf("TEST PRINT :: FINAL :: RESETTED :: i   =    %d     ::   finalIndex    =    %d \n", i , finalIndex);


					// STORING UP THE PATH STRINGS TO ARRAY
					strcpy(target[i-1], singlePath);

					free(singlePath);	
				}
				
			}


			// --------
			// DEBUG
			// for(int i=1; i<=searchedFileCount; i++) {
			// 	printf("TEST PRINT :: FINAL :: RESETTED :: target[ %d ]      		::    %s \n", i-1 , target[i-1]);
			// }




		} else {

			// ----------------------------------------------------
			// < NORMAL ANIMATION >
			//
			// CREATING FILE LIST
			for(int i=1; i<=searchedFileCount; i++) {

				if(target[i-1] != NULL) {
					// CREATING SINGLE IMAGE FILE PATH (ONE LINE)
					char* singlePath;
					singlePath = malloc(sizeof(char) * MAXFILENAMELENGTH);


					// CALCULATING FINAL INDEX
					int finalIndex = accumulatedStartIndex + (i-1);
					printf("finalIndex    ::        %d      \n", finalIndex);



					char startIndexChar[30];
					size_t startIndexCharLetters = 0;

					sprintf(startIndexChar, "%d", finalIndex);
					printf("startIndexChar    ::        %s      \n", startIndexChar);


					// SECOND, WE COUNT THE LETTERS OF STARTING INDEX CHAR
					startIndexCharLetters = strlen(startIndexChar);
					printf("startIndexCharLetters    ::        %d      \n", startIndexCharLetters);



					int zeroCnt = MAX_CHAR_OF_INDEX - startIndexCharLetters;
					printf("zeroCnt    ::        %d      \n", zeroCnt);



					// --------------------------------
					// CREATING ZERO LETTERS
					char zeroLetters[zeroCnt];

					for(int j=0; j<zeroCnt; j++) {
						zeroLetters[j] = '0';
					}
					zeroLetters[zeroCnt] = '\0';



					// FORMATTING PATH OF STRING FOR SINGLE FILE
					sprintf(singlePath, "%spg/pg%s%d.png", fileLocation, zeroLetters, finalIndex);

					printf("TEST PRINT :: FINAL :: DISPLAYING SCENE     i   =    %d     ::   zeroLetters    =    %s \n", i , zeroLetters);
					printf("TEST PRINT :: FINAL :: DISPLAYING SCENE     i   =    %d     ::   finalIndex     =    %d \n", i , finalIndex);

					// STORING UP THE PATH STRINGS TO ARRAY
					strcpy(target[i-1], singlePath);

					free(singlePath);	
				}
				
			}


			// --------
			// DEBUG
			// for(int i=1; i<=searchedFileCount; i++) {
			// 	printf("TEST PRINT :: FINAL :: DISPLAYING SCENE :: target[ %d ]      		::    %s \n", i-1 , target[i-1]);
			// }

		}


	    fclose(csvFile);

	    csvFile = NULL;



	} else {


		printf(" **** CSV FILE IS NULL !!! \n");

	}


	free(csvPath);
}




void setupFonts() {

	// set font file
	FontxFile fx16G[2];
	FontxFile fx24G[2];
	FontxFile fx32G[2];
	FontxFile fx32L[2];
	InitFontx(fx16G,"/spiffs/font/ILGH16XB.FNT",""); // 8x16Dot Gothic
	InitFontx(fx24G,"/spiffs/font/ILGH24XB.FNT",""); // 12x24Dot Gothic
	InitFontx(fx32G,"/spiffs/font/ILGH32XB.FNT",""); // 16x32Dot Gothic
	InitFontx(fx32L,"/spiffs/font/LATIN32B.FNT",""); // 16x32Dot Latinc

	FontxFile fx16M[2];
	FontxFile fx24M[2];
	FontxFile fx32M[2];
	InitFontx(fx16M,"/spiffs/font/ILMH16XB.FNT",""); // 8x16Dot Mincyo
	InitFontx(fx24M,"/spiffs/font/ILMH24XB.FNT",""); // 12x24Dot Mincyo
	InitFontx(fx32M,"/spiffs/font/ILMH32XB.FNT",""); // 16x32Dot Mincyo

}




void setupSPIDisplay() {

	#if CONFIG_XPT2046	
	ESP_LOGI(TAG_PngSlide, "Enable XPT2046 Touch Contoller");
	int MISO_GPIO = CONFIG_MISO_GPIO;
	int XPT_CS_GPIO = CONFIG_XPT_CS_GPIO;
	int XPT_IRQ_GPIO = CONFIG_XPT_IRQ_GPIO;
	#else	
	int MISO_GPIO = -1;
	int XPT_CS_GPIO = -1;
	int XPT_IRQ_GPIO = -1;
	#endif	


	printf("---- INITIALIZATION OF SPI....\n");
	// SPI INITIALIZE
	spi_master_init(&pngSlideInstance.dev, 
					CONFIG_MOSI_GPIO, 
					CONFIG_SCLK_GPIO, 
					CONFIG_TFT_CS_GPIO, 
					CONFIG_DC_GPIO, 
					CONFIG_RESET_GPIO, 
					CONFIG_BL_GPIO, 
					MISO_GPIO, 
					XPT_CS_GPIO, 
					XPT_IRQ_GPIO);
	printf("---- INITIALIZATION OF SPI....   OK\n");




	#if CONFIG_ILI9340
	uint16_t model = 0x9340;
	#endif
	#if CONFIG_ILI9341
	uint16_t model = 0x9341;
	#endif


	printf("---- INITIALIZATION OF LCD....\n");
	lcdInit(&pngSlideInstance.dev, 
			model, 
			CONFIG_WIDTH, 
			CONFIG_HEIGHT, 
			CONFIG_OFFSETX, 
			CONFIG_OFFSETY);
	printf("---- INITIALIZATION OF LCD....   OK\n");




	#if CONFIG_INVERSION
	ESP_LOGI(TAG_PngSlide, "Enable Display Inversion");
	lcdInversionOn(&pngSlideInstance.dev);
	#endif

	#if CONFIG_RGB_COLOR
	ESP_LOGI(TAG_PngSlide, "Change BGR filter to RGB filter");
	lcdBGRFilter(&pngSlideInstance.dev);
	#endif


	//---------------------------------------------------------


	lcdSetFontDirection(&pngSlideInstance.dev, 0);
	lcdFillScreen(&pngSlideInstance.dev, BLACK);


}



void setupFileList() {

	// PREPARING MEMORY AREA
	// MEMORY ALLOCATING FOR FILE LIST
    for(int i=0; i<MAXFILENUM; i++) {
    	pngSlideInstance.hPngAnim.fileList[i] = malloc(sizeof(char) * MAXFILENAMELENGTH);	
    }


	printf("pngSlideInstance.currentGroupIndex  ::     %d      \n", pngSlideInstance.currentGroupIndex);


	// CONSTRUCTING FILE LIST OF CURRENT SCENE
	makeFileNameList(pngSlideInstance.hPngAnim.fileList, 
					 pngSlideInstance.currentGroupIndex);

	// CALCULATING IMAGE COUNT OF SINGLE SCENE
	pngSlideInstance.hPngAnim.imageCounts = getFileListCount(pngSlideInstance.hPngAnim.fileList, 
															 pngSlideInstance.currentGroupIndex);


	printf("pngSlideInstance.hPngAnim.imageCounts  ::     %d      \n", pngSlideInstance.hPngAnim.imageCounts);




}



// groupIndexToPlay IS "1" ORIGIN
PngSlide_t* PngSlideStart(int groupIndexToPlay) {

	printf("-- pngSlideInstance SIZE  ::  %d\r\n", sizeof(pngSlideInstance));


	// SETTING GROUP INDEX NUMBER TO PLAY
	pngSlideInstance.currentGroupIndex = groupIndexToPlay;


	// -------------------------------------------------------------
	
	setupFileList();
	setupFonts();
	setupSPIDisplay();



	//---------------------------------------------------------
	// < INITIALIZING PNG HANDLE >
	engineInit(&pngSlideInstance);
	


	playFrame(&pngSlideInstance.dev, pngSlideInstance.hPngAnim.pngle, CONFIG_WIDTH, CONFIG_HEIGHT);




	pngSlideInstance.hPngAnim.currentFrame++;




	return &pngSlideInstance;

}








// 
void PngSlide_Task_Play(void *pvParameters) {

	// **** ABOVE PARAMETER IS THE GLOBAL INSTANCE OF pngSlideInstance ****
	//      IT IS THE SAME THING ! 
	//      !!!! SO DO NOT USE pngSlideInstance HERE !!!!
	// 
	// INSTEAD USE BELOW 'pngSlide' !!!!!!!  ***********************



	// CONVERT 'BACK' TO PROPER TYPE
	PngSlide_t* pngSlide = (PngSlide_t*) pvParameters;

	gUartDt = 0;
	gUartDtLast = 0;



	printf("===================================================\r\n");
	printf("PngSlide_Task_Play STARTED!!!! \n");


	while(1) {

		gHEAPMEMORY_REMAINED = heap_caps_get_free_size(MALLOC_CAP_8BIT);
		printf("===================================================\r\n");
		printf("MEMORY CHECKING REMAINED     ::     PNGSLIDE      ::  %d\r\n", gHEAPMEMORY_REMAINED);


		
		//ESP_ERROR_CHECK( heap_trace_start(HEAP_TRACE_LEAKS) );


		// ---------------------------------------------------------
		// ---------------------------------------------------------
		// < COMMUNICATION WITH WIFI + UART MODULE >

		//char* uartDt = uartRxRead();
		//gUartDt = atoi(uartDt);
		gUartDt = 0;
		
		//printf("  ****  pngSlide  ****   gUartDt   IS  ----    %d  \r\n", gUartDt);
		

		// -------------------------------
		// < NEW SCENE NUMBER ADJUSTING >
		// CALCULATING DIFFERENCES BETWEEN 
		// TRANSFERRED SCENE NUMBER AND CURRENT SCENE NUMBER
		//int imgGrpDiff = gUartDt - gUartDtLast;
		int imgGrpDiff = 0;

		// IF DIFFERENCE IS MINUS, MULTIPLY BY -1
		if(imgGrpDiff < 0) imgGrpDiff *= -1;
		//printf("  ****  pngSlide  ****   imgGrpDiff   IS  ----    %d  \r\n", imgGrpDiff);




		if(imgGrpDiff > 0) {

			printf("=============================\r\n");
			printf("   NEW UART DATA IS COMING ::      %d\r\n", gUartDt);
			printf("=============================\r\n");

			printf("==~=~=--  RESET TO NEW IMG-GRP ~=~~=~=\r\n");

			// ---------------------------------------------
			// RE-NEW CURRENT FILE LIST
			pngSlide->currentGroupIndex = gUartDt;


		    // REMAKE FILE LIST OF IMAGE GROUP
			makeFileNameList(pngSlide->hPngAnim.fileList, 
							 pngSlide->currentGroupIndex);


			pngSlide->hPngAnim.imageCounts = getFileListCount(pngSlide->hPngAnim.fileList, 
															  pngSlide->currentGroupIndex);

			printf("pngSlide->hPngAnim.imageCounts  ::     %d      \n", pngSlide->hPngAnim.imageCounts);




			// ---------------------------------------------
			// RESET TO NEW PLAYBACK
			if(pngSlide->hPngAnim.fileCurrent != NULL) {
				fclose(pngSlide->hPngAnim.fileCurrent);
			}

			pngSlide->hPngAnim.currentFrame = 0;


			FILE* imageCurrent 		= fopen(pngSlide->hPngAnim.fileList[0], "rb");

			// TRANSFERRING POINTER TO FILE TO PngAnim OBJECT
			pngSlide->hPngAnim.fileCurrent = imageCurrent;
			imageCurrent = NULL;


			//printf("-- FILES ARE RESETTED -- \r\n");


		}




		// ---------------------------------------------------------
		// ---------------------------------------------------------
		// ---------------------------------------------------------
		// ---------------------------------------------------------




		// UPDATING NEW UartData
		gUartDtLast = gUartDt;





		// -------------------------------------------------------------------------------------------
		printf("====  ALL FRAME COUNT ::  imageCounts  ::     %d      \n", pngSlide->hPngAnim.imageCounts);


		if( pngSlide->hPngAnim.imageCounts != 0 ) {

			if( pngSlide->hPngAnim.currentFrame < pngSlide->hPngAnim.imageCounts ) {

				readyNewBuffer(pngSlide->hPngAnim.fileCurrent, pngSlide->hPngAnim.pngle);

				printf("@@ PLAYING\r\n");
				printf("-- PLAYING      FRAME NUMBER   ::  %d\n", pngSlide->hPngAnim.currentFrame);
				printf("                FILE NAME      ::  %s\n", pngSlide->hPngAnim.fileList[pngSlide->hPngAnim.currentFrame]);

				PngSlidePlayCurrentFrame(&pngSlide->dev, pngSlide->hPngAnim.pngle, CONFIG_WIDTH, CONFIG_HEIGHT);

				pngSlide->hPngAnim.currentFrame++;

				// printf("-- PLAYING ...  NEXT  FRAME NUMBER     			    ::     %d      \n", pngSlide->hPngAnim.currentFrame);

				readyNextImgFile(&pngSlide->hPngAnim, pngSlide->hPngAnim.currentFrame);


			} else {

				// < RESET AND LOOP >

				printf("\n==~=~=--=============~=~~=~=\n");
				printf("==~=~=--  RESET !!!! ~=~~=~=\r\n");

				// // CLOSING FILE
				// printf("CLOSING CURRENT FILE ....     \r\n");
				// if(pngSlide->hPngAnim.fileCurrent != NULL) {
				// 	fclose(pngSlide->hPngAnim.fileCurrent);
				// 	printf("CLOSING CURRENT FILE ....     DONE\r\n");
				// }



				pngSlide->hPngAnim.currentFrame = 0;

				// RETURN CURRENT FRAME TO FIRST IMAGE ( [0] INDEXED )
				printf("OPENING FIRST IMAGE ....     \r\n");
				FILE* imageCurrent 		= fopen(pngSlide->hPngAnim.fileList[0], "rb");
				printf("OPENING FIRST IMAGE ....     OK\r\n");



				// TRANSFERRING POINTER TO FILE TO PngAnim OBJECT
				pngSlide->hPngAnim.fileCurrent = imageCurrent;
				imageCurrent = NULL;

			}


		} else {

			printf("====  ALL FRAME COUNT   ::  imageCounts   ::     IS     0    !!!      \n");

		}






		// if(&pngSlide->hPngAnim != NULL) {

	 	//  //printf("===================================================\r\n");
		// 	//printf("@@ PLAY_A  ::   LOADING ... \r\n");

		//  //printf("TIMER_SCALE   ::    %d\r\n", TIMER_SCALE);					// 10000hz

		




		vTaskDelay(1);

	}


}



void PngSlide_Task_Play_Start(PngSlide_t* pngSlide, int scale, int priority, int cpuCoreInd) {

	xTaskCreatePinnedToCore(PngSlide_Task_Play, "PngSlide_Task_Play", 1024*scale, pngSlide, priority, NULL, cpuCoreInd);
	//xTaskCreate(PngSlide_Task_Play, "PngSlide_Task_Play", 1024*scale, pngSlide, priority, NULL);

}







// void resetSequence(pngleAnim_t* _pngleAnim) {
// 	pngle_reset(_pngleAnim->pngle);

// 	if(_pngleAnim->fileCurrent != NULL) {
// 		fclose(_pngleAnim->fileCurrent);

// 	}

// 	_pngleAnim->currentFrame = 0;

// }








// bool PngSlideClose(pngleAnim_t* pngleAnim, int canvWidth, int canvHeight) {
// 	// FOR SHORTCUT
// 	pngle_t* _pngle = pngleAnim->pngle;

// 	printf("-- BEFORE CLEARING pngle \n");
// 	pngle_destroy(_pngle, canvWidth, canvHeight);


// 	// DEALLCOATING FILE OBJECTS
// 	free(pngleAnim->fileCurrent);
// 	//free(pngleAnim->fileNext);

// 	// DEALLOCATING pngleAnim STRUCT
// 	free(pngleAnim);


// 	return 0;
// }









// int GetGrpImgCount() {
// 	return pngSlideInstance.hPngAnim.imgList.imgCount;
// }

// void PngSlideAddImgGroup(const char** imgs) {

// 	// ALLOCATING MEMORY FOR NEW IMAGE GROUP
// 	pngSlideInstance.hPngAnim.imgList.InitList();

// 	int listLength = calcSizeCharArray(imgs);
// 	pngSlideInstance.hPngAnim.imgList.imgCount = listLength;

// 	for(int i=0; i<listLength; i++) {

// 		// ADDING SINGLE GROUP OF IMAGE
// 		pngSlideInstance.hPngAnim.imgList.SetSingleImage( imgs[i] , i);
// 	}
// }





// // RESETTING FILE LIST
// // fList WILL BE pngSlide->hPngAnim.fileList
// void resetFileList(char** fList) {
// }







// // YOU CAN ADD IMAGE USING INDEX EITHER
// void SetSingleImage(const char* inputImg, int ind) {

// 	strcpy(pngSlideInstance.hPngAnim.imgList.imgGroup[ind], inputImg);

// }





// void CloseList() {

// 	// < FREEING MEMORY OF ARRAY IN ARRAY >
// 	// https://www.quora.com/How-do-I-free-an-array-in-C

// 	for(int i=0; i<pngSlideInstance.hPngAnim.imgList.imgCount; i++) {
// 		free(pngSlideInstance.hPngAnim.imgList.imgGroup[i]);
// 	}
// 	free(pngSlideInstance.hPngAnim.imgList.imgGroup);

// 	pngSlideInstance.hPngAnim.imgList.imgCount = 0;
// 	pngSlideInstance.hPngAnim.imgList.grpIndex = -1;

// }
