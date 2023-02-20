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
#include <HX711_ADC.h>

#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
	#include <EEPROM.h>
#endif

/************************************************************
************************************************************/
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
************************************************************/
/********************
modify this area, when you want to charnge NUM_SENSORS.
********************/
enum{
	NUM_SENSORS = 2,
};

SENSOR_ASSIGN SensorAssign[NUM_SENSORS] = {
	SENSOR_ASSIGN(4, 5, 0),
	SENSOR_ASSIGN(6, 7, 4),
};

HX711_ADC LoadCell[NUM_SENSORS] = {
	HX711_ADC(SensorAssign[0].HX711_dout, SensorAssign[0].HX711_sck),
	HX711_ADC(SensorAssign[1].HX711_dout, SensorAssign[1].HX711_sck),
};

float calibrationValue[NUM_SENSORS] = {696.0, 733.0}; // will be overwritten with the value in EEPROM

/********************
********************/
unsigned long t_from = 0;

/********************
********************/
enum STATE{
	STATE_WARMING,
	STATE_RUN,
};
STATE State = STATE_WARMING;
unsigned long t_Statefrom = 0;


/************************************************************
func
************************************************************/

/******************************
******************************/
void setup() {
	/********************
	********************/
	Serial.begin(57600);
	delay(10);
	Serial.println();
	Serial.println("Starting...");

	/********************
	********************/
	for(int i = 0; i < NUM_SENSORS; i++){
		LoadCell[i].begin();
		// LoadCell[i].setReverseOutput();	//uncomment to turn a negative output value to positive
		LoadCell[i].setSamplesInUse(2);	// 1, 2, 4, 8, 16, 32, 64 or 128.
	}
	
#if defined(ESP8266) || defined(ESP32)
	//EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch the value from eeprom
#endif
	for(int i = 0; i < NUM_SENSORS; i++){
		EEPROM.get(SensorAssign[i].calVal_eepromAdress, calibrationValue[i]);
	}
	
	unsigned long stabilizingtime = 2000;	// tare preciscion can be improved by adding a few seconds of stabilizing time
	boolean _tare = true;					// set this to false if you don't want tare to be performed in the next step
	
	byte loadcell_rdy[NUM_SENSORS];
	for(int i = 0; i < NUM_SENSORS; i++) { loadcell_rdy[i] = 0; }
	
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
			char buf[100];
			sprintf(buf, "Timeout, check MCU>HX711 No.[%d] wiring and pin designations", i);
			Serial.println(buf);
			
			while(1);
		}
	}
	
	for(int i = 0; i < NUM_SENSORS; i++){
		LoadCell[i].setCalFactor(calibrationValue[i]);
	}
	
	Serial.println("Startup is complete");
	
	/********************
	********************/
	t_Statefrom = millis();
}

/******************************
******************************/
void loop() {
	/********************
	********************/
	switch(State){
		case STATE_WARMING:
			if(1000 < millis() - t_Statefrom){
				State = STATE_RUN;
			}
			break;
			
		case STATE_RUN:
			break;
	}
	
	/********************
	********************/
	static boolean newDataReady = false;
	const int serialPrintInterval = 0;
	
	if (LoadCell[0].update()) newDataReady = true;
	for(int i = 1; i < NUM_SENSORS; i++){
		LoadCell[i].update();
	}
	
	if (newDataReady) {
		if (serialPrintInterval < millis() - t_from) {
			float Weight_g[NUM_SENSORS];
			for(int i = 0; i < NUM_SENSORS; i++) { Weight_g[i] = LoadCell[i].getData(); }
			
			if(State == STATE_RUN){
			#if false
				float TotalWeight = 0;
				for(int i = 0; i < NUM_SENSORS; i++) { TotalWeight += Weight_g[i]; }
				
				float Height = 500;
				Serial.print( (millis() / 1000) % 2 * Height );
				Serial.print(",");
				for(int i = 0; i < NUM_SENSORS; i++){
					Serial.print(Weight_g[i]);
					Serial.print(",");
				}
				Serial.println(TotalWeight);
			#else
				Serial.println("--------------------");
				
				/********************
				Arduinoでsprintfの書式設定
					https://kurobekoblog.com/arduino_sprint
				********************/
				for(int i = 0; i < NUM_SENSORS; i++){
					char buf_temp[100];
					dtostrf(Weight_g[i], 10, 2, buf_temp); // dtostrf(浮動小数点値,文字列の長さ,小数点以下の桁数,文字列バッファ)
					
					char buf[100];
					sprintf(buf, "LoadCell %d output val = %s", i, buf_temp);
					Serial.println(buf);
				}
			#endif
			}
			
			/* */
			newDataReady = false;
			t_from = millis();
		}
	}
	
	/********************
	********************/
	if (0 < Serial.available()) {
		char inByte = Serial.read();
		if (inByte == 't') {
			for(int i = 0; i < NUM_SENSORS; i++) { LoadCell[i].tareNoDelay(); }
		}
	}
	
	/********************
	********************/
	for(int i = 0; i < NUM_SENSORS; i++){
		if (LoadCell[i].getTareStatus() == true) {
			char buf[100];
			sprintf(buf, "Tare load cell %d complete", i);
			Serial.println(buf);
		}
	}
}
