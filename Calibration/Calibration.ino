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

#define LOADCELL_ID 0


/************************************************************
************************************************************/

/********************
********************/
#if (LOADCELL_ID == 0)
	const int HX711_dout	= 4;
	const int HX711_sck		= 5;
	
	const int calVal_eepromAdress = 0;
#elif (LOADCELL_ID == 1)
	const int HX711_dout	= 6;
	const int HX711_sck		= 7;

	const int calVal_eepromAdress = 4;
#elif (LOADCELL_ID == 2)
	const int HX711_dout	= 8;
	const int HX711_sck		= 9;

	const int calVal_eepromAdress = 8;
#elif (LOADCELL_ID == 3)
	const int HX711_dout	= 10;
	const int HX711_sck		= 11;

	const int calVal_eepromAdress = 12;
#endif

/********************
********************/
HX711_ADC LoadCell(HX711_dout, HX711_sck);
unsigned long t_from = 0;

boolean b_Disp = false;

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
	LoadCell.begin();
	//LoadCell.setReverseOutput();			// uncomment to turn a negative output value to positive
	
	/********************
	********************/
	unsigned long stabilizingtime = 2000;	// preciscion right after power-up can be improved by adding a few seconds of stabilizing time
	boolean _tare = true;					// set this to false if you don't want tare to be performed in the next step
	LoadCell.start(stabilizingtime, _tare);
	if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
		Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
		while (1);
	}else{
		LoadCell.setCalFactor(1.0);			// user set calibration value (float), initial value 1.0 may be used for this sketch
		Serial.println("Startup is complete");
	}
	
	/********************
	********************/
	while (!LoadCell.update());
	
	calibrate();
}

/******************************
******************************/
void loop() {
	/********************
	********************/
	static boolean newDataReady = 0;
	const int serialPrintInterval = 0;
	
	/********************
	********************/
	if (LoadCell.update()) newDataReady = true;
	
	/********************
	********************/
	if (newDataReady) {
		if (serialPrintInterval < millis() - t_from) {
			float i = LoadCell.getData();
			if(b_Disp){
				Serial.print("Load_cell output val: ");
				Serial.println(i);
			}
			newDataReady = 0;
			t_from = millis();
		}
	}
	
	/********************
	********************/
	if (0 < Serial.available()) {
		char inByte = Serial.read();
		if (inByte == 't')		LoadCell.tareNoDelay(); //tare
		else if (inByte == 'r') calibrate(); //calibrate
		else if (inByte == 'c') changeSavedCalFactor(); //edit calibration value manually
		else if (inByte == 'd') b_Disp = !b_Disp;
	}
	
	/********************
	check if last tare operation is complete
	********************/
	if (LoadCell.getTareStatus() == true) {
		Serial.println("Tare complete");
	}
}

/******************************
******************************/
void calibrate() {
	Serial.println("***");
	Serial.println("Start calibration:");
	Serial.println("Place the load cell on a level stable surface.");
	Serial.println("Remove any load applied to the load cell.");
	Serial.println("Send 't' from serial monitor to set the tare offset.");
	
	boolean _resume = false;
	while (_resume == false) {
		LoadCell.update();
		
		if (0 < Serial.available()) {
			char inByte = Serial.read();
			if (inByte == 't') LoadCell.tareNoDelay();
		}
		
		if (LoadCell.getTareStatus() == true) {
			Serial.println("Tare complete");
			_resume = true;
		}
	}
	
	Serial.println("Now, place your known mass on the loadcell.");
	Serial.println("Then send the weight of this mass[g] (i.e. 100.0) from serial monitor.");
	
	float known_mass = 0;
	_resume = false;
	while (_resume == false) {
		LoadCell.update();
		
		if (0 < Serial.available()) {
			known_mass = Serial.parseFloat();
			if (known_mass != 0) {
				Serial.print("Known mass[g] is: ");
				Serial.println(known_mass);
				_resume = true;
			}
		}
	}
	
	LoadCell.refreshDataSet();											// refresh the dataset to be sure that the known mass is measured correct
	float newCalibrationValue = LoadCell.getNewCalibration(known_mass);	// get the new calibration value. setCalFactor() is called in getNewCalibration() function(SJ).
	
	Serial.print("New calibration value has been set to: ");
	Serial.print(newCalibrationValue);
	Serial.println(", use this as calibration value (calFactor) in your project sketch.");
	Serial.print("Save this value to EEPROM adress ");
	Serial.print(calVal_eepromAdress);
	Serial.println("? y/n");
	
	_resume = false;
	while (_resume == false) {
		if (0 < Serial.available()) {
			char inByte = Serial.read();
			if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
				EEPROM.begin(512);
#endif
				EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
				EEPROM.commit();
#endif
				EEPROM.get(calVal_eepromAdress, newCalibrationValue);
				Serial.print("Value ");
				Serial.print(newCalibrationValue);
				Serial.print(" saved to EEPROM address: ");
				Serial.println(calVal_eepromAdress);
				_resume = true;
			}else if (inByte == 'n') {
				Serial.println("Value not saved to EEPROM");
				_resume = true;
			}
		}
	}
	
	Serial.println("End calibration");
	Serial.println("***");
	Serial.println("To re-calibrate, send 'r' from serial monitor.");
	Serial.println("For manual edit of the calibration value, send 'c' from serial monitor.");
	Serial.println("***");
}

/******************************
******************************/
void changeSavedCalFactor() {
	float oldCalibrationValue = LoadCell.getCalFactor();
	boolean _resume = false;
	Serial.println("***");
	Serial.print("Current value is: ");
	Serial.println(oldCalibrationValue);
	Serial.println("Now, send the new value from serial monitor, i.e. 696.0");
	float newCalibrationValue;
	while (_resume == false) {
		if (0 < Serial.available()) {
			newCalibrationValue = Serial.parseFloat();
			if (newCalibrationValue != 0) {
				Serial.print("New calibration value is: ");
				Serial.println(newCalibrationValue);
				LoadCell.setCalFactor(newCalibrationValue);
				_resume = true;
			}
		}
	}
	_resume = false;
	Serial.print("Save this value to EEPROM adress ");
	Serial.print(calVal_eepromAdress);
	Serial.println("? y/n");
	while (_resume == false) {
		if (0 < Serial.available()) {
			char inByte = Serial.read();
			if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
				EEPROM.begin(512);
#endif
				EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
				EEPROM.commit();
#endif
				EEPROM.get(calVal_eepromAdress, newCalibrationValue);
				Serial.print("Value ");
				Serial.print(newCalibrationValue);
				Serial.print(" saved to EEPROM address: ");
				Serial.println(calVal_eepromAdress);
				_resume = true;
			}
			else if (inByte == 'n') {
				Serial.println("Value not saved to EEPROM");
				_resume = true;
			}
		}
	}
	Serial.println("End change calibration value");
	Serial.println("***");
}
