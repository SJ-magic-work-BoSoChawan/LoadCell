/************************************************************
■original note
	HX711_ADC
	Arduino library for HX711 24-Bit Analog-to-Digital Converter for Weight Scales
	Olav Kallhovd sept2017
	
	
	Settling time (number of samples) and data filtering can be adjusted in the config.h file
	For calibration and storing the calibration value in eeprom, see example file "Calibration.ino"
	
	The update() function checks for new data and starts the next conversion. In order to acheive maximum effective
	sample rate, update() should be called at least as often as the HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS.
	If you have other time consuming code running (i.e. a graphical LCD), consider calling update() from an interrupt routine,
	see example file "Read_1x_load_cell_interrupt_driven.ino".
	
	This is an example sketch on how to use this library
************************************************************/
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
	#include <EEPROM.h>
#endif

/************************************************************
************************************************************/
const int HX711_dout	= 4;
const int HX711_sck		= 5;

HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t_from = 0;

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
	LoadCell.begin();
	LoadCell.setReverseOutput();	// uncomment to turn a negative output value to positive
	LoadCell.setSamplesInUse(2);	// 1, 2, 4, 8, 16, 32, 64 or 128.
	
	float calibrationValue;				// calibration value (see example file "Calibration.ino")
	calibrationValue = 696.0;			// uncomment this if you want to set the calibration value in the sketch
#if defined(ESP8266)|| defined(ESP32)
	//EEPROM.begin(512);								// uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif
	EEPROM.get(calVal_eepromAdress, calibrationValue);	// uncomment this if you want to fetch the calibration value from eeprom

	unsigned long stabilizingtime = 2000;	// preciscion right after power-up can be improved by adding a few seconds of stabilizing time
	boolean _tare = true;					// set this to false if you don't want tare to be performed in the next step
	LoadCell.start(stabilizingtime, _tare);
	if (LoadCell.getTareTimeoutFlag()) {
		Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
		while (1);
	}else{
		LoadCell.setCalFactor(calibrationValue);
		Serial.println("Startup is complete");
	}
	
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
	static boolean newDataReady = false; // LoadCellはupdateされたが、表示intervalによってskipされた場合、同intervalが過ぎた後、表示するので、static.
	const int serialPrintInterval = 0;
	
	/********************
	********************/
	if (LoadCell.update()) newDataReady = true;
	
	/********************
	********************/
	if (newDataReady) {
		if (serialPrintInterval < millis() - t_from) {
			/* */
			float Weight_g = LoadCell.getData();
			
			if(State == STATE_RUN){
				float Height = 500;
				Serial.print( (millis() / 1000) % 2 * Height );
				Serial.print(",");
				Serial.println(Weight_g);
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
		if (inByte == 't') LoadCell.tareNoDelay();
	}
	
	if (LoadCell.getTareStatus() == true) {
		Serial.println("Tare complete");
	}
}

