/************************************************************
■original note
	HX711_ADC
	Arduino library for HX711 24-Bit Analog-to-Digital Converter for Weight Scales
	Olav Kallhovd sept2017
	
	
	This example file shows how to calibrate the load cell and optionally store the calibration
	value in EEPROM, and also how to change the value manually.
	The result value can then later be included in your project sketch or fetched from EEPROM.
	
	To implement calibration in your project sketch the simplified procedure is as follow:
		LoadCell.tare();
		//place known mass
		LoadCell.refreshDataSet();
		float newCalibrationValue = LoadCell.getNewCalibration(known_mass);

■ArduinoのEEPROMを使う
	http://7ujm.net/micro/arduino_eeprom.html#:~:text=Arduino%20UNO%E3%81%AA%E3%81%A9%E3%81%AB%E4%BD%BF,%E3%82%82%E4%BF%9D%E5%AD%98%E3%81%A7%E3%81%8D%E3%82%8B%E3%81%9D%E3%81%86%E3%81%A7%E3%81%99%E3%80%82
		ATmega328(Arduino UNO) :	1k (1024) Byte = 8 bit data x 1024 個
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
#define NUM_SENSORS 4

#if (NUM_SENSORS == 1)
	SENSOR_ASSIGN SensorAssign[NUM_SENSORS] = {
		SENSOR_ASSIGN( 2,  3,  0),
	};
	
	HX711_ADC LoadCell[NUM_SENSORS] = {
		HX711_ADC(SensorAssign[0].HX711_dout, SensorAssign[0].HX711_sck),
	};
	
	// float calibrationValue[NUM_SENSORS] = {696.0}; // will be overwritten with the value in EEPROM
#elif (NUM_SENSORS == 2)
	SENSOR_ASSIGN SensorAssign[NUM_SENSORS] = {
		SENSOR_ASSIGN( 2,  3,  0),
		SENSOR_ASSIGN( 5,  6,  4),
	};
	
	HX711_ADC LoadCell[NUM_SENSORS] = {
		HX711_ADC(SensorAssign[0].HX711_dout, SensorAssign[0].HX711_sck),
		HX711_ADC(SensorAssign[1].HX711_dout, SensorAssign[1].HX711_sck),
	};
	
	// float calibrationValue[NUM_SENSORS] = {696.0, 733.0}; // will be overwritten with the value in EEPROM
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
	
	// float calibrationValue[NUM_SENSORS] = {696.0, 733.0, 696.0, 733.0}; // will be overwritten with the value in EEPROM
#endif

/********************
********************/
long t_from = 0;
boolean b_Disp = false;

/********************
********************/
enum STATE{
	STATE_WARMING,
	STATE_RUN,
};
STATE State = STATE_WARMING;
long t_Statefrom = 0;

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
	Serial.print("NumSensors=");
	Serial.println(NUM_SENSORS);
	Serial.println("Starting...");
	Serial.println();
	
	/********************
	********************/
	for(int i = 0; i < NUM_SENSORS; i++){
		LoadCell[i].begin();
		// LoadCell[i].setReverseOutput();	//uncomment to turn a negative output value to positive
		// LoadCell[i].setSamplesInUse(15);
	}
	
	unsigned long stabilizingtime = 2000;	// preciscion right after power-up can be improved by adding a few seconds of stabilizing time
	boolean _tare = true;					// set this to false if you don't want tare to be performed in the next step
	for(int i = 0; i < NUM_SENSORS; i++){
		LoadCell[i].start(stabilizingtime, _tare);
		if (LoadCell[i].getTareTimeoutFlag() || LoadCell[i].getSignalTimeoutFlag()) {
			char buf[50];
			sprintf(buf, "[%d]Timeout, check wiring and pin designations", i);
			Serial.println(buf);
			
			while (1);
		}else{
			LoadCell[i].setCalFactor(1.0);			// user set calibration value (float), initial value 1.0 may be used for this sketch
			char buf[50];
			sprintf(buf, "[%d]Startup OK", i);
			println_Result(buf);
		}
		
		
		while (!LoadCell[i].update());
		
		calibrate(i);
	}
	
	/********************
	********************/
	print_CalFactor();
}

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
	static boolean newDataReady = false;
	const long serialPrintInterval = 0;
	
	if (LoadCell[0].update()) newDataReady = true;
	for(int i = 1; i < NUM_SENSORS; i++){ // 注) i = 1 番目以降をcheckしている
		LoadCell[i].update();
	}
	
	if (newDataReady) {
		if (serialPrintInterval < my_millis() - t_from) {
			float Weight_g[NUM_SENSORS];
			for(int i = 0; i < NUM_SENSORS; i++) { Weight_g[i] = LoadCell[i].getData(); }
			
			if(b_Disp) { print_AllWeight(Weight_g, NUM_SENSORS); }
			
			/* */
			newDataReady = false;
			t_from = my_millis();
		}
	}
	
	/********************
	********************/
	if (0 < Serial.available())	keyPressed(Serial.read());
	
	/********************
	********************/
	for(int i = 0; i < NUM_SENSORS; i++){
		if (LoadCell[i].getTareStatus() == true) {
			char buf[50];
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
void print_CalFactor() {
	for(int i = 0; i < NUM_SENSORS; i++){
		char buf_temp[100];
		dtostrf(LoadCell[i].getCalFactor(), 10, 2, buf_temp); // dtostrf(浮動小数点値,文字列の長さ,小数点以下の桁数,文字列バッファ)
		
		char buf[50];
		sprintf(buf, "[%d]CalFactor=%s", i, buf_temp);
		Serial.println(buf);
	}
}

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
			
		case 'c':
			print_CalFactor();
			break;
			
		case 'r':
			for(int i = 0; i < NUM_SENSORS; i++) { calibrate(i); }
			break;
			
		case 'd':
			b_Disp = !b_Disp;
			break;
	}
}

/******************************
******************************/
void print_Result(String _str_message){
	String str_message = "-> ";
	str_message += _str_message;
	Serial.print(str_message);
}

/******************************
******************************/
void println_Result(String _str_message){
	String str_message = "-> ";
	str_message += _str_message;
	Serial.println(str_message);
}

/******************************
******************************/
void print_AllWeight(float Weight_g[], int NumSensors) {
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
}

/******************************
******************************/
void calibrate(int id) {
	/********************
	********************/
	if(NUM_SENSORS <= id){
		printError(__LINE__);
		return;
	}
	
	/********************
	********************/
	Serial.println();
	Serial.println("***");
	char buf[50];
	sprintf(buf, "[%02d]StartCalib:", id);
	Serial.println(buf);
	Serial.println("Place the load cell on a level stable surface.");
	Serial.println("Remove any load.");
	Serial.println("Send 't' from serial monitor to set the tare offset.");
	
	boolean _resume = false;
	while (_resume == false) {
		LoadCell[id].update();
		
		if (0 < Serial.available()) {
			char inByte = Serial.read();
			if (inByte == 't') LoadCell[id].tareNoDelay();
		}
		
		if (LoadCell[id].getTareStatus() == true) {
			println_Result("Tare OK");
			Serial.println();
			_resume = true;
		}
	}
	
	Serial.println("\Place your known mass.");
	Serial.println("Then send the weight of this mass[g] (i.e. 100.0).");
	
	float known_mass = 0;
	_resume = false;
	while (_resume == false) {
		LoadCell[id].update();
		
		if (0 < Serial.available()) {
			known_mass = Serial.parseFloat();
			if (known_mass != 0) {
				print_Result("Known mass[g] is: ");
				Serial.println(known_mass);
				_resume = true;
			}
		}
	}
	
	LoadCell[id].refreshDataSet();											// refresh the dataset to be sure that the known mass is measured correct
	float newCalibrationValue = LoadCell[id].getNewCalibration(known_mass);	// get the new calibration value. setCalFactor() is called in getNewCalibration() function(SJ).
	
	Serial.println();
	Serial.print("New calibration value has been set to: ");
	Serial.print(newCalibrationValue);
	Serial.println(", use this as calFactor in your sketch.");
	Serial.print("Save to EEPROM adress [");
	Serial.print(SensorAssign[id].calVal_eepromAdress);
	Serial.println("]? y/n");
	
	_resume = false;
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
	// Serial.println("-> To re-calibrate, send 'r' from serial monitor.");
	// Serial.println("-> For manual edit of the calibration value, send 'm' from serial monitor.");
	Serial.println("***");
	Serial.println();
}

/******************************
******************************/
void ManualChange_CalFactor(int id) {
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
	Serial.println("***");
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
	Serial.println("***");
}
