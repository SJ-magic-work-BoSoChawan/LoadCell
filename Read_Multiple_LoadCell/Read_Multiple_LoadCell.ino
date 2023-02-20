/************************************************************
■original note
	HX711_ADC.h
	Arduino master library for HX711 24-Bit Analog-to-Digital Converter for Weigh Scales
	Olav Kallhovd sept2017
	Tested with			: HX711 asian module on channel A and YZC-133 3kg load cell
	Tested with MCU	: Arduino Nano
	
	This is an example sketch on how to use this library for two ore more HX711 modules
	Settling time (number of samples) and data filtering can be adjusted in the config.h file
************************************************************/

/************************************************************
Compile switch
************************************************************/
#define __COMMUNICATION__
#define __USE_OLED__

// #define __LOG_RECENT__

// #define __CHECK_EACH_SENSOR__
// #define __SD_LOG__



/************************************************************
SO1602
************************************************************/
#ifdef __USE_OLED__
	#include <SO1602.h>
	
	#define SO1602_ADDR 0x3C
	SO1602 oled(SO1602_ADDR);
#endif

/************************************************************
SD Log : by using ethernet shield
************************************************************/
#ifdef __SD_LOG__
	#include <SPI.h>
	#include <SD.h>
	
	#define SD_SS 4
	char* filename = "L.csv"; // 8文字.3文字 以内
	long t_SD = 0;
	long SD_interval = 100000;
	
	const int LED_PIN = 9;	// 外付Led : showing start process complete.
	
	/* for 異常値 */
	char* filename_outlier = "O.csv"; // 8文字.3文字 以内
	
#else
	const int LED_PIN = 13; // 内蔵Led : showing start process complete.
	
#endif

/************************************************************
HX711
************************************************************/
#include <HX711_ADC.h>

#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
	#include <EEPROM.h>
#endif

struct SENSOR_ASSIGN{
	const int HX711_dout;
	const int HX711_sck;
	const int calVal_eepromAdress;
	
	SENSOR_ASSIGN(int _HX711_dout, int _HX711_sck, int _calVal_eepromAdress)
	: HX711_dout(_HX711_dout), HX711_sck(_HX711_sck), calVal_eepromAdress(_calVal_eepromAdress)
	{
	}
};

/************************************************************
param
************************************************************/
/********************
modify this area, when you want to charnge NUM_SENSORS.
********************/
#ifdef __SD_LOG__
	#define NUM_SENSORS 2 // 1, 2, or 4 : Log取る場合は、NUM_SENSORS <= 2 とすること(otherwise, you will get build error.)
#else
	#define NUM_SENSORS 4 // 1, 2, or 4
#endif

#if (NUM_SENSORS == 1)
	SENSOR_ASSIGN SensorAssign[NUM_SENSORS] = {
		SENSOR_ASSIGN( 2,  3,  0),
	};
	
	HX711_ADC LoadCell[NUM_SENSORS] = {
		HX711_ADC(SensorAssign[0].HX711_dout, SensorAssign[0].HX711_sck),
	};
	
	float calibrationValue[NUM_SENSORS] = {696.0}; // will be overwritten with the value in EEPROM
#elif (NUM_SENSORS == 2)
	SENSOR_ASSIGN SensorAssign[NUM_SENSORS] = {
		SENSOR_ASSIGN( 2,  3,  0),
		SENSOR_ASSIGN( 5,  6,  4),
	};
	
	HX711_ADC LoadCell[NUM_SENSORS] = {
		HX711_ADC(SensorAssign[0].HX711_dout, SensorAssign[0].HX711_sck),
		HX711_ADC(SensorAssign[1].HX711_dout, SensorAssign[1].HX711_sck),
	};
	
	float calibrationValue[NUM_SENSORS] = {696.0, 733.0}; // will be overwritten with the value in EEPROM
#elif(NUM_SENSORS == 4)
	SENSOR_ASSIGN SensorAssign[NUM_SENSORS] = {
		SENSOR_ASSIGN( 2,  3,  0),
		SENSOR_ASSIGN( 5,  6,  4),
		SENSOR_ASSIGN( 7,  8,  8),
		SENSOR_ASSIGN( 9, 10, 12),
	};
	
	HX711_ADC LoadCell[NUM_SENSORS] = {
		HX711_ADC(SensorAssign[0].HX711_dout, SensorAssign[0].HX711_sck),
		HX711_ADC(SensorAssign[1].HX711_dout, SensorAssign[1].HX711_sck),
		HX711_ADC(SensorAssign[2].HX711_dout, SensorAssign[2].HX711_sck),
		HX711_ADC(SensorAssign[3].HX711_dout, SensorAssign[3].HX711_sck),
	};
	
	float calibrationValue[NUM_SENSORS] = {696.0, 733.0, 696.0, 733.0}; // will be overwritten with the value in EEPROM
#endif


/********************
********************/
long t_SerialPrintInterval_from = 0;
boolean b_Disp = false;

long t_ActualProcessInterval = 0;
long c_OffTheTable = 0;

/********************
********************/
enum class RUNNING_STATE{
	WARMING,
	RUN,
};
RUNNING_STATE RunningState = RUNNING_STATE::WARMING;
long t_RunningStatefrom = 0;


/************************************************************
Send SensorState to music board through I2C
************************************************************/
#include <Wire.h>

#define SLAVE_ADDRESS 0x08

enum class SENSOR_STATE{
	ON_THE_TABLE,
	OFF_THE_TABLE,
	SUSPECT_OFS_ERROR,
	WILL_RESET,
	
	RESETTING,
};

SENSOR_STATE SensorState = SENSOR_STATE::ON_THE_TABLE;
long t_SensorStatefrom = 0;
int c_ofs_NG = 0;
int c_ofs_OK = 0;

/************************************************************
************************************************************/
#ifdef __LOG_RECENT__
	struct RECENT_LOG{
		long time;
		float Weight[NUM_SENSORS];
		int SensorState;
		
		RECENT_LOG(){
			time = -1;
			for(int i = 0; i < NUM_SENSORS; i++){
				Weight[i] = 0;
			}
			SensorState = -1;
		}
		
		void set(long now, float _Weight_g[], int _SensorState){
			time = now;
			
			for(int i = 0; i < NUM_SENSORS; i++){
				Weight[i] = _Weight_g[i];
			}
			
			SensorState = _SensorState;
		}
		
		void print( long time_ofs = 0 ){
			Serial.print(time);
			Serial.print(",");
			Serial.print(time - time_ofs);
			Serial.print(",");
			for(int i = 0; i < NUM_SENSORS; i++){
				Serial.print(Weight[i]);
				Serial.print(",");
			}
			Serial.println(SensorState);
		}
	};
	
	enum{
		NUM_RECENT_LOGS = 14,
	};
	
	RECENT_LOG RecentLogs[NUM_RECENT_LOGS];
	
	enum class RECENT_LOG_STATE{
		STOP,
		COUNT_TIMER,
	};
	
	RECENT_LOG_STATE RecentLogState = RECENT_LOG_STATE::STOP;
	long t_RecentLogStatefrom = 0;
	
	int c_LogRecent = 0;
#endif

/************************************************************
check if you have set the compile switch correctly.
************************************************************/
#ifdef __SD_LOG__
	#if(2 < NUM_SENSORS)
		#error please set "NUM_SENSORS" <= 2 in log mode.
	#endif
	
	#ifdef __COMMUNICATION__
		#error log & communication not allowed at the same time.
	#endif
#endif

/************************************************************
function prototype : for Reset
	■Arduinoをリセットするいくつかの方法！【RESET端子/ソフトウェアリセット/ウォッチドッグタイマー(WDT)】
		https://burariweb.info/electronic-work/arduino-reset-procedure.html#i
		
	■Arduinoでコードからリセットをする方法
		https://rb-station.com/blogs/article/arduino-software-reset
************************************************************/
void(* resetFunc) (void) = 0;


/************************************************************
func
************************************************************/

/******************************
******************************/
void setup() {
	/********************
	********************/
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, LOW);
	
	/********************
	********************/
	Serial.begin(57600);
	delay(10);
	Serial.println();
	Serial.print("NumSensors:");
	Serial.println(NUM_SENSORS);
	Serial.println("Starting...");
	
	Wire.begin(); // I2C
	
	/********************
	********************/
	for(int i = 0; i < NUM_SENSORS; i++){
		LoadCell[i].begin();
		// LoadCell[i].setReverseOutput();	//uncomment to turn a negative output value to positive
		LoadCell[i].setSamplesInUse(7);
	}
	
#if defined(ESP8266) || defined(ESP32)
	EEPROM.begin(512);						// uncomment this if you use ESP8266 and want to fetch the value from eeprom
#endif
	for(int i = 0; i < NUM_SENSORS; i++){
		EEPROM.get(SensorAssign[i].calVal_eepromAdress, calibrationValue[i]);
	}
	
	unsigned long stabilizingtime = 2000;	// tare preciscion can be improved by adding a few seconds of stabilizing time
	boolean _tare = true;					// set this to false if you don't want tare to be performed in the next step
	
	byte loadcell_rdy[NUM_SENSORS];
	for(int i = 0; i < NUM_SENSORS; i++) { loadcell_rdy[i] = 0; }	// init.
	
	byte NumStarted = 0;
	while(NumStarted < NUM_SENSORS){		// run startup, stabilization and tare, both modules simultaniously
		for(int i = 0; i < NUM_SENSORS; i++){
			if(!loadcell_rdy[i]) loadcell_rdy[i] = LoadCell[i].startMultiple(stabilizingtime, _tare);
		}
		
		NumStarted = 0;
		for(int i = 0; i < NUM_SENSORS; i++){
			if(loadcell_rdy[i]) NumStarted++;
		}
	}
	
	for(int i = 0; i < NUM_SENSORS; i++){
		if (LoadCell[i].getTareTimeoutFlag()) {
			printError(__LINE__);
			while(1);
		}
	}
	
	for(int i = 0; i < NUM_SENSORS; i++){
		LoadCell[i].setCalFactor(calibrationValue[i]);
	}
	
	/********************
	********************/
	t_RunningStatefrom	= my_millis();
	t_SensorStatefrom	= my_millis();
	
	/********************
	OLED
	********************/
#ifdef __USE_OLED__
	init_oled();
#endif
	
	/********************
	SD
	********************/
#ifdef __SD_LOG__
	pinMode(SS, OUTPUT);
	pinMode(SD_SS, OUTPUT);
	digitalWrite(SS, HIGH);
	digitalWrite(SD_SS, LOW);
	
	if( !SD.begin(SD_SS) ) { printError(__LINE__); while(1); }
#endif
	
	Serial.println("Startup OK");
}


/******************************
******************************/
#ifdef __USE_OLED__
void init_oled(){
	oled.begin();
	oled.set_cursol(0); // cursor 非表示
	oled.set_blink(0);
	oled.clear();
	
	oled.move(0, 0);
	oled.charwrite("BoSo Chawan");
}
#endif


/******************************
******************************/
/*
void printError(char* chFileName, int line, char* chFuncName){
	Serial.println("> Error");
	Serial.println(chFileName);
	
	Serial.print("line:");
	Serial.println(line);
	
	Serial.print("func:");
	Serial.println(chFuncName);
}
*/

/******************************
******************************/
/*
void printError(char* chFileName, int line){
	Serial.println("> Error");
	Serial.println(chFileName);
	
	Serial.print("line:");
	Serial.println(line);
}
*/

/******************************
******************************/
void printError(int line){
	Serial.println("> Error");
	Serial.print("line:");
	Serial.println(line);
}

/******************************
******************************/
void loop() {
	/********************
	********************/
	StateChart_Running();
	
	/********************
	********************/
	static boolean newDataReady = false;
	const long serialPrintInterval = 0; // ms
	
	if (LoadCell[0].update()) newDataReady = true;
	for(int i = 1; i < NUM_SENSORS; i++){ // 注) i = 1 番目以降をcheckしている
		LoadCell[i].update();
	}
	
	/********************
	********************/
	if (newDataReady) {
		if(RunningState == RUNNING_STATE::RUN){
			if (serialPrintInterval < my_millis() - t_SerialPrintInterval_from) { // 動作速度 = 表示速度 : 見えているモノが動いているモノ
				float Weight_g[NUM_SENSORS];
				for(int i = 0; i < NUM_SENSORS; i++) { Weight_g[i] = LoadCell[i].getData(); }
				
				if(b_Disp){
					// print_plotter(Weight_g, NUM_SENSORS);
					
					print_AllWeight(Weight_g, NUM_SENSORS);
					// get_and_print_slave_info();
				}
				
#ifdef __SD_LOG__
				Log_OutlierWeight_to_SD(Weight_g, NUM_SENSORS);
				Log_Weight_to_SD(Weight_g, NUM_SENSORS);
#endif
				
				/* */
				StateChart_Sensor(Weight_g, NUM_SENSORS);
				StateChart_RecentLogs(Weight_g);
				
				/* */
				Send_SensorState_to_MusicBoard();
				update_oled(Weight_g, NUM_SENSORS);
				
				t_ActualProcessInterval = my_millis() - t_SerialPrintInterval_from;
				newDataReady = false;
				t_SerialPrintInterval_from = my_millis();
			}
		}else{
			newDataReady = false; // RUNでない時は、スルー.
		}
	}
	
	/********************
	********************/
	if (0 < Serial.available())	keyPressed(Serial.read());
	
	/********************
	********************/
	for(int i = 0; i < NUM_SENSORS; i++){
		if (LoadCell[i].getTareStatus() == true) {
			char buf[100];
			sprintf(buf, "[%d]Tare OK", i);
			Serial.println(buf);
		}
	}
	
	/********************
	sensor : 10 sample/sec : 100 ms/sample
	********************/
	delay(10);
}

/******************************
******************************/
long my_millis() {
	return (long)millis();
}

/******************************
******************************/
void Send_SensorState_to_MusicBoard(){
#ifdef __COMMUNICATION__
	Wire.beginTransmission(SLAVE_ADDRESS);
	Wire.write((uint8_t)SensorState);
	Wire.endTransmission();
#endif
}

/******************************
******************************/
void StateChart_RecentLogs(float Weight_g[]) {
#ifdef __LOG_RECENT__
	/********************
	********************/
	for(int i = 0; i < NUM_RECENT_LOGS - 1; i++){
		RecentLogs[i] = RecentLogs[i + 1];
	}
	
	RecentLogs[NUM_RECENT_LOGS - 1].set(my_millis(), Weight_g, (int)SensorState);
	
	/********************
	********************/
	switch(RecentLogState){
		case RECENT_LOG_STATE::STOP:
			break;
			
		case RECENT_LOG_STATE::COUNT_TIMER:
		{
			const long SensorInterval_ms = 100;
			
			if(NUM_RECENT_LOGS / 2 * SensorInterval_ms < my_millis() - t_RecentLogStatefrom){
				print_RecentLog();
				RecentLogState = RECENT_LOG_STATE::STOP;
			}
		}
			break;
	}
#endif
}

/******************************
******************************/
#ifdef __LOG_RECENT__
void print_RecentLog(){
	Serial.print(c_LogRecent);
	Serial.println("]");
	
	for(int i = 0; i < NUM_RECENT_LOGS; i++){
		RecentLogs[i].print( RecentLogs[0].time );
	}
	c_LogRecent++;
}
#endif

/******************************
******************************/
int get_UpdateTimesOfSec(float sec){
	return (int)(sec * 1000 / 120); // HX711の測定intervalは 100msだが、実測で 120[ms] 程度なので
}

/******************************
******************************/
void StateChart_Sensor(float Weight_g[], int NumSensors) {
	/********************
	********************/
	float TotalWeight = 0;
	for(int i = 0; i < NumSensors; i++) { TotalWeight += Weight_g[i]; }
	
	/********************
	th_to__OFF は、茶碗がなくなる方向であり、手で押すなどの間違いが少ない(引っ張られることは、物理的に考えて難しい方向)。
	なので、やや攻めた値で行こう。
	2023.01.25
	********************/
	// const float Weight_of_Bowl		= 600;	// #0 : チクチク特攻隊長
	const float Weight_of_Bowl		= 500;	// #1 : 
	// const float Weight_of_Bowl		= 500;	// #2 : 
	// const float Weight_of_Bowl		= 716;	// #3 : 
	// const float Weight_of_Bowl		= 400;	// #4 : なめ猫初代総長又吉
	
	const float th_to__ON				= -Weight_of_Bowl * 0.4;	// 200
	const float th_to__OFF				= -Weight_of_Bowl * 0.5;	// 250
	const float th_AllowedOffset_Large	=  Weight_of_Bowl * 0.2;	// 100
	const float th_AllowedOffset_Small	=  Weight_of_Bowl * 0.14;	// 70
	
	/********************
	********************/
	switch(SensorState){
		case SENSOR_STATE::ON_THE_TABLE:
			if( TotalWeight < th_to__OFF ) {
				SensorState = SENSOR_STATE::OFF_THE_TABLE;
				t_SensorStatefrom = my_millis();
				c_OffTheTable++;
				
#ifdef __LOG_RECENT__
				RecentLogState = RECENT_LOG_STATE::COUNT_TIMER;
				t_RecentLogStatefrom = my_millis();
#endif
			}else{
			
#ifndef __CHECK_EACH_SENSOR__
				if( abs(TotalWeight) < th_AllowedOffset_Large ){
					c_ofs_NG = 0;
				}else{
					c_ofs_NG++;
					if( get_UpdateTimesOfSec(30.0) < c_ofs_NG ){
						SensorState = SENSOR_STATE::SUSPECT_OFS_ERROR;
						t_SensorStatefrom = my_millis();
						c_ofs_NG = 0;
						c_ofs_OK = 0;
					}
				}
#endif
			
			}
			
			break;
			
		case SENSOR_STATE::OFF_THE_TABLE:
			if( th_to__ON < TotalWeight )	{ SensorState = SENSOR_STATE::ON_THE_TABLE; c_ofs_NG = 0; c_ofs_OK = 0; t_SensorStatefrom = my_millis(); }
			
			break;
			
		case SENSOR_STATE::SUSPECT_OFS_ERROR:
			if( abs(TotalWeight) < th_AllowedOffset_Small ){
				c_ofs_OK++;
				c_ofs_NG = 0;
				
				if( get_UpdateTimesOfSec(5.0) < c_ofs_OK ){
					SensorState = SENSOR_STATE::ON_THE_TABLE;
					t_SensorStatefrom = my_millis();
					c_ofs_OK = 0;
					c_ofs_NG = 0;
				}
			}else{
				c_ofs_NG++;
				c_ofs_OK = 0;
				
				if(get_UpdateTimesOfSec(20.0) <  c_ofs_NG){
					SensorState = SENSOR_STATE::WILL_RESET;
					t_SensorStatefrom = my_millis();
				}
			}
			
			break;
			
		case SENSOR_STATE::WILL_RESET:
			if(30000 < my_millis() - t_SensorStatefrom){ // ここは、counterでなく、直接 時間
#ifdef __LOG_RECENT__
				Serial.println("Reset");
				delay(100);
#endif
				
				SensorState = SENSOR_STATE::RESETTING;
				t_SensorStatefrom = my_millis();
				
				resetFunc();
			}
			break;
			
		case SENSOR_STATE::RESETTING:
			break;
	}
}

/******************************
******************************/
void StateChart_Running(){
	switch(RunningState){
		case RUNNING_STATE::WARMING:
			if(1000 < my_millis() - t_RunningStatefrom){
				RunningState = RUNNING_STATE::RUN;
				digitalWrite(LED_PIN, HIGH);
			}
			break;
			
		case RUNNING_STATE::RUN:
			break;
	}
}

/******************************
******************************/
void update_oled_eachSensor(float Weight_g[], int NumSensors) {
	static float Large_Weight_g[NUM_SENSORS] = {0, 0, 0, 0};
	bool b_update_Large = false;
	
	static float Small_Weight_g[NUM_SENSORS] = {0, 0, 0, 0};
	bool b_update_Small = false;
	
	for(int i = 0; i < NumSensors; i++) {
		if( Large_Weight_g[i] < Weight_g[i] ){
			Large_Weight_g[i] = Weight_g[i];
			b_update_Large = true;
		}
		
		if( Weight_g[i] < Small_Weight_g[i] ){ 
			Small_Weight_g[i] = Weight_g[i];
			b_update_Small = true;
		}
	}
	
	enum{ NUM_COLUMNS_OLED = 16, };
	String str_Oled;
	char buf[NUM_COLUMNS_OLED + 1]; // + 終端文字
	
	if(b_update_Large){
		str_Oled = "";
		
		for(int i = 0; i < NumSensors; i++) {
			sprintf(buf, "%4d", (int)Large_Weight_g[i]);
			str_Oled += buf;
		}
		
		/********************
		********************/
		char ch_Oled[NUM_COLUMNS_OLED + 1]; // + 終端文字
		str_Oled.toCharArray(ch_Oled, NUM_COLUMNS_OLED + 1);
		
		oled.move(0, 0);
		oled.charwrite(ch_Oled);
	}
	
	if(b_update_Small){
		str_Oled = "";
		
		for(int i = 0; i < NumSensors; i++) {
			sprintf(buf, "%4d", (int)Small_Weight_g[i]);
			str_Oled += buf;
		}
		
		/********************
		********************/
		char ch_Oled[NUM_COLUMNS_OLED + 1]; // + 終端文字
		str_Oled.toCharArray(ch_Oled, NUM_COLUMNS_OLED + 1);
		
		oled.move(0, 1);
		oled.charwrite(ch_Oled);
	}
}

/******************************
******************************/
void update_oled(float Weight_g[], int NumSensors) {
	
#ifdef __CHECK_EACH_SENSOR__
	update_oled_eachSensor(Weight_g, NumSensors);
	return;
#endif
	
	
	
	
#ifdef __USE_OLED__
	/********************
	********************/
	float TotalWeight = 0;
	for(int i = 0; i < NumSensors; i++) { TotalWeight += Weight_g[i]; }
	
	/********************
	********************/
	enum{ NUM_COLUMNS_OLED = 16, };
	
	String str_Oled;
	
	char buf[NUM_COLUMNS_OLED + 1]; // + 終端文字
	sprintf(buf, "%5d ", (int)TotalWeight);
	str_Oled += buf;
	
#ifdef __LOG_RECENT__
	sprintf(buf, "%3d ", (int)c_OffTheTable);
#else
	sprintf(buf, "%3d ", (int)t_ActualProcessInterval);
#endif
	str_Oled += buf;
	
	/********************
	********************/
#ifdef __COMMUNICATION__
	enum{NUM_REQUESTS = 2,};
	
	Wire.requestFrom(SLAVE_ADDRESS, NUM_REQUESTS);
	if(Wire.available() <= 0){
		str_Oled += "------";
	}else{
		uint8_t val[NUM_REQUESTS];
		for(int i = 0; (i < NUM_REQUESTS) && (0 < Wire.available()); i++) { val[i] = Wire.read(); }
		
		 // SensorState
		sprintf(buf, "%d ", val[0]);
		str_Oled += buf;
		
		 // Music State
		switch(val[1]){
			case 0:
				sprintf(buf, "Stop");
				break;
			case 1:
				sprintf(buf, "V_up");
				break;
			case 2:
				sprintf(buf, "MaxP");
				break;
			case 3:
				sprintf(buf, "V_dn");
				break;
			case 4:
				sprintf(buf, "Paus");
				break;
			case 5:
				sprintf(buf, "WRes");
				break;
			case 6:
				sprintf(buf, "aRes");
				break;
			case 7:
				sprintf(buf, "sOfs");
				break;
			case 8:
				sprintf(buf, "aOfs");
				break;
			case 9:
				sprintf(buf, "wRst");
				break;
			case 10:
				sprintf(buf, "aRst");
				break;
			case 11:
				sprintf(buf, "dRcv");
				break;
			case 12:
				sprintf(buf, "aRcv");
				break;
		}
		
		str_Oled += buf;
	}
	
#endif // #ifdef __COMMUNICATION__
	
	/********************
	********************/
    char ch_Oled[NUM_COLUMNS_OLED + 1]; // + 終端文字
    str_Oled.toCharArray(ch_Oled, NUM_COLUMNS_OLED + 1);
	
	oled.move(0, 1);
	oled.charwrite(ch_Oled);
	
#endif // #ifdef __USE_OLED__
}

/******************************
******************************/
void get_and_print_slave_info(){
#ifdef __COMMUNICATION__
	Wire.requestFrom(SLAVE_ADDRESS, 2);
	Serial.print("Slave:(Sensor,Music)=");
	while(0 < Wire.available()){
		uint8_t val = Wire.read();
		Serial.print(val);
		Serial.print(",");
	}
	Serial.println();
#endif
}

/******************************
******************************/
#ifdef __SD_LOG__
void Log_OutlierWeight_to_SD(float Weight_g[], int NumSensors){
	/********************
	********************/
	static float Last_Weight_g[NUM_SENSORS];
	static boolean b_1st = true;
	
	/********************
	********************/
	if(b_1st){
		b_1st = false;
		for(int i = 0; i < NUM_SENSORS; i++) { Last_Weight_g[i] = 0; }
	}
	
	/********************
	********************/
	boolean b_Log = false;
	const float thresh = 10.0f;
	for(int i = 0; i < NUM_SENSORS; i++){
		if( thresh < abs(Weight_g[i] - Last_Weight_g[i]) ) { b_Log = true; break; }
	}
	
	/********************
	********************/
	if(b_Log){
		File TargetFile = SD.open(filename_outlier, FILE_WRITE);
		if(TargetFile){
			TargetFile.print(my_millis());
			TargetFile.print(",");
			for(int i = 0; i < NUM_SENSORS; i++){
				TargetFile.print(Weight_g[i]);
				TargetFile.print(",");
				
				Last_Weight_g[i] = Weight_g[i];
			}
			TargetFile.println();
			
			TargetFile.close();
		}else{
			printError(__LINE__);
		}
	}
}
#endif

/******************************
******************************/
#ifdef __SD_LOG__
void Log_Weight_to_SD(float Weight_g[], int NumSensors){
	if(SD_interval < my_millis() - t_SD){
		t_SD = my_millis();
		
		File TargetFile = SD.open(filename, FILE_WRITE);
		if(TargetFile){
			TargetFile.print(my_millis());
			TargetFile.print(",");
			for(int i = 0; i < NumSensors; i++){
				TargetFile.print(Weight_g[i]);
				TargetFile.print(",");
			}
			TargetFile.println();
			
			TargetFile.close();
			// Serial.println("fwrite");
		}else{
			printError(__LINE__);
		}
	}
}
#endif

/******************************
******************************/
void keyPressed(char key){
	switch(key){
		case 't':
			for(int i = 0; i < NUM_SENSORS; i++) { LoadCell[i].tareNoDelay(); }
			break;
			
		case 'm':
			for(int i = 0; i < NUM_SENSORS; i++) { ManualChange_CalFactor(i); }
			break;
			
		case 'i':
			Serial.println(t_ActualProcessInterval);
			break;
			
		case 'c':
			print_CalFactor();
			break;
			
		case 'd':
			b_Disp = !b_Disp;
			break;
			
		case 's':
			print_SensorState();
			break;
			
		case 'g':
			get_and_print_slave_info();
			break;
			
		case 'r':
			resetFunc();
			break;
	}
}

/******************************
******************************/
void print_SensorState(){
#ifndef __LOG_RECENT__
	Serial.print("SensorState:");
	Serial.println((int)SensorState);
#endif
}

/******************************
******************************/
void print_Result(String _str_message){
#if defined(__SD_LOG__) || defined(__LOG_RECENT__)
	// nothing
#else
	String str_message = "-> ";
	str_message += _str_message;
	Serial.print(str_message);
#endif
}

/******************************
******************************/
void print_sepatation(){
#if defined(__SD_LOG__) || defined(__LOG_RECENT__)
	// nothing
#else
	for(int i = 0; i < 3; i++){
		Serial.print("*");
	}
	
	Serial.println();
#endif
}

/******************************
******************************/
void println_Result(String _str_message){
#if defined(__SD_LOG__) || defined(__LOG_RECENT__)
	// nothing
#else
	String str_message = "-> ";
	str_message += _str_message;
	Serial.println(str_message);
#endif
}

/******************************
******************************/
void print_CalFactor() {
#ifndef __LOG_RECENT__
	for(int i = 0; i < NUM_SENSORS; i++){
		char buf_temp[100];
		dtostrf(LoadCell[i].getCalFactor(), 10, 2, buf_temp); // dtostrf(浮動小数点値,文字列の長さ,小数点以下の桁数,文字列バッファ)
		
		char buf[100];
		sprintf(buf, "CalFactor[%d]=%s", i, buf_temp);
		Serial.println(buf);
	}
#endif
}

/******************************
******************************/
void print_AllWeight(float Weight_g[], int NumSensors) {
#ifndef __LOG_RECENT__
	for(int i = 0; i < 20; i++) { Serial.print("-"); }
	Serial.println();
	
	/********************
	Arduinoでsprintfの書式設定
		https://kurobekoblog.com/arduino_sprint
	********************/
	for(int i = 0; i < NumSensors; i++){
		char buf_temp[100];
		dtostrf(Weight_g[i], 10, 2, buf_temp); // dtostrf(浮動小数点値,文字列の長さ,小数点以下の桁数,文字列バッファ)
		
		char buf[50];
		sprintf(buf, "[%d] %s[g]", i, buf_temp);
		Serial.println(buf);
	}
#endif
}

/******************************
******************************/
void print_plotter(float Weight_g[], int NumSensors) {
#ifndef __LOG_RECENT__
	float TotalWeight = 0;
	for(int i = 0; i < NumSensors; i++) { TotalWeight += Weight_g[i]; }
	
	float Height = -500;
	Serial.print( (my_millis() / 1000) % 2 * Height );
	Serial.print(",");
	for(int i = 0; i < NumSensors; i++){
		Serial.print(Weight_g[i]);
		Serial.print(",");
	}
	Serial.println(TotalWeight);
#endif
}

/******************************
Log mode時のRAMが足りないので、Log modeで、本関数は空にした(2023.01.06 Sj)
******************************/
void ManualChange_CalFactor(int id) {
#if defined(__SD_LOG__) || defined(__LOG_RECENT__)
	// nothing.
	
#else
	/********************
	********************/
	if(NUM_SENSORS <= id){
		printError(__LINE__);
		return;
	}
	
	/********************
	********************/
	float oldCalibrationValue = LoadCell[id].getCalFactor();
	
	boolean _resume = false;
	print_sepatation();
	char buf[50];
	sprintf(buf, "[%02d]Current: ", id);
	Serial.print(buf);
	Serial.println(oldCalibrationValue);
	Serial.println("Send the new value from serial monitor, i.e. 696.0");
	
	float newCalibrationValue;
	while (_resume == false) {
		if (0 < Serial.available()) {
			newCalibrationValue = Serial.parseFloat();
			if (newCalibrationValue != 0) {
				print_Result("New calibration value is: ");
				Serial.println(newCalibrationValue);
				Serial.println();
				LoadCell[id].setCalFactor(newCalibrationValue);
				_resume = true;
			}
		}
	}
	
	_resume = false;
	Serial.print("Save to EEPROM adress [");
	Serial.print(SensorAssign[id].calVal_eepromAdress);
	Serial.println("]? y/n");
	while (_resume == false) {
		if (0 < Serial.available()) {
			char inByte = Serial.read();
			if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
				EEPROM.begin(512);
#endif
				EEPROM.put(SensorAssign[id].calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
				EEPROM.commit();
#endif
				EEPROM.get(SensorAssign[id].calVal_eepromAdress, newCalibrationValue);
				print_Result("Value ");
				Serial.print(newCalibrationValue);
				Serial.print(" saved to EEPROM address: ");
				Serial.println(SensorAssign[id].calVal_eepromAdress);
				_resume = true;
			}else if (inByte == 'n') {
				println_Result("Value not saved");
				_resume = true;
			}
		}
	}
	
	Serial.println();
	sprintf(buf, "[%02d]End", id);
	Serial.println(buf);
	print_sepatation();
#endif
}
