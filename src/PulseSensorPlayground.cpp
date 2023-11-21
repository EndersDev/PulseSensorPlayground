/*
   A central Playground object to manage a set of PulseSensors.
   See https://www.pulsesensor.com to get started.

   Copyright World Famous Electronics LLC - see LICENSE
   Contributors:
     Joel Murphy, https://pulsesensor.com
     Yury Gitman, https://pulsesensor.com
     Bradford Needham, @bneedhamia, https://bluepapertech.com

   Licensed under the MIT License, a copy of which
   should have been included with this software.

   This software is not intended for medical use.
*/
#include <PulseSensorPlayground.h>

// Define the "this" pointer for the ISR
PulseSensorPlayground *PulseSensorPlayground::OurThis;


PulseSensorPlayground::PulseSensorPlayground(int numberOfSensors) {
  // Save a static pointer to our playground so the ISR can read it.
  OurThis = this;

  // Dynamically create the array to minimize ram usage.
  SensorCount = (byte) numberOfSensors;
  Sensors = new PulseSensor[SensorCount];

#if PULSE_SENSOR_TIMING_ANALYSIS
  // We want sample timing analysis, so we construct it.
  pTiming = new PulseSensorTimingStatistics(MICROS_PER_READ, 500 * 30L);
#endif // PULSE_SENSOR_TIMING_ANALYSIS
}

boolean PulseSensorPlayground::PulseSensorPlayground::begin() {

  for (int i = 0; i < SensorCount; ++i) {
    Sensors[i].initializeLEDs();
  }

  // Note the time, for non-interrupt sampling and for timing statistics.
  NextSampleMicros = micros() + MICROS_PER_READ;

  SawNewSample = false;
	Paused = false;

#if PULSE_SENSOR_MEMORY_USAGE
  // Report the RAM usage and hang.
  printMemoryUsage();
  for (;;);
#endif // PULSE_SENSOR_MEMORY_USAGE

// #if defined (ARDUINO_ARCH_SAM)
// void sampleTimer(){
//   onSampleTime();  // PulseSensorPlayground::OurThis->onSampleTime();
// }
// #endif

  // Lastly, set up and turn on the interrupts.
  if (USE_ARDUINO_INTERRUPTS){ // (UsingInterrupts) {
    if (!PulseSensorPlaygroundSetupInterrupt()) {
			Paused = true;
      return false;
    }
  }
  /*
    If you want to measure the time it takes to run our algorithm,
    uncomment this line and all the other timingPin lines. 
    Then connect timingPin to an oscilloscope. The pin will be HIGH
    during the duration of our algorithm.

  */
  // pinMode(timingPin,OUTPUT); // optionally connect timingPin to oscilloscope to time algorithm run time
  return true;
}

void PulseSensorPlayground::analogInput(int inputPin, int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return; // out of range.
  }
  Sensors[sensorIndex].analogInput(inputPin);
}

void PulseSensorPlayground::blinkOnPulse(int blinkPin, int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return; // out of range.
  }
  Sensors[sensorIndex].blinkOnPulse(blinkPin);
}

void PulseSensorPlayground::fadeOnPulse(int fadePin, int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return; // out of range.
  }
  Sensors[sensorIndex].fadeOnPulse(fadePin);
}

boolean PulseSensorPlayground::sawNewSample() {
  /*
     If using interrupts, this function reads and clears the
     'saw a sample' flag that is set by the ISR.

     When not using interrupts, this function sees whether it's time
     to sample and, if so, reads the sample and processes it.

		 First, check to see if the sketch has paused the Pulse Sensor sampling
  */
  if(!Paused){
    if (USE_ARDUINO_INTERRUPTS){ // (UsingInterrupts) {
      // Disable interrupts to avoid a race with the ISR.
      // DISABLE_PULSE_SENSOR_INTERRUPTS;
      boolean sawOne = SawNewSample;
      SawNewSample = false;
      // ENABLE_PULSE_SENSOR_INTERRUPTS;

      return sawOne;
    } else {  // Time the sample as close as you can when not using interrupts
      unsigned long nowMicros = micros();
      if ((long) (NextSampleMicros - nowMicros) > 0L) {
        return false;  // not time yet.
      }
      NextSampleMicros = nowMicros + MICROS_PER_READ;

    #if PULSE_SENSOR_TIMING_ANALYSIS
      if (pTiming->recordSampleTime() <= 0) {
        pTiming->outputStatistics(SerialOutput.getSerial());
        for (;;); // Hang because we've disturbed the timing.
      }
    #endif // PULSE_SENSOR_TIMING_ANALYSIS

      // time to call the sample processor
      onSampleTime();
      return true;
  	}
  }
  
  return false;
  
}

void PulseSensorPlayground::onSampleTime() {
  // Typically called from the ISR at 500Hz
  // digitalWrite(timingPin,HIGH); // optionally connect timingPin to oscilloscope to time algorithm run time
  /*
     Read the voltage from each PulseSensor.
     We do this separately from processing the samples
     to minimize jitter in acquiring the signal.
  */
  for (int i = 0; i < SensorCount; ++i) {
    Sensors[i].readNextSample();
  }

  // Process those samples.
  for (int i = 0; i < SensorCount; ++i) {
    Sensors[i].processLatestSample();
    Sensors[i].updateLEDs();
  }

  // Set the flag that says we've read a sample since the Sketch checked.
  SawNewSample = true;

  // digitalWrite(timingPin,LOW); // optionally connect timingPin to oscilloscope to time algorithm run time
 }

int PulseSensorPlayground::getLatestSample(int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return -1; // out of range.
  }
  return Sensors[sensorIndex].getLatestSample();
}

int PulseSensorPlayground::getBeatsPerMinute(int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return -1; // out of range.
  }
  return Sensors[sensorIndex].getBeatsPerMinute();
}

int PulseSensorPlayground::getInterBeatIntervalMs(int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return -1; // out of range.
  }
  return Sensors[sensorIndex].getInterBeatIntervalMs();
}

boolean PulseSensorPlayground::sawStartOfBeat(int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return false; // out of range.
  }
  return Sensors[sensorIndex].sawStartOfBeat();
}

boolean PulseSensorPlayground::isInsideBeat(int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return false; // out of range.
  }
  return Sensors[sensorIndex].isInsideBeat();
}

void PulseSensorPlayground::setThreshold(int threshold, int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return; // out of range.
  }
  Sensors[sensorIndex].setThreshold(threshold);
}

#if USE_SERIAL

  void PulseSensorPlayground::setSerial(Stream &output) {
    SerialOutput.setSerial(output);
  }

  void PulseSensorPlayground::setOutputType(byte outputType) {
    SerialOutput.setOutputType(outputType);
  }

  void PulseSensorPlayground::outputSample() {
    SerialOutput.outputSample(Sensors, SensorCount);
  }

  void PulseSensorPlayground::outputBeat(int sensorIndex) {
    SerialOutput.outputBeat(Sensors, SensorCount, sensorIndex);
  }

  void PulseSensorPlayground::outputToSerial(char s, int d) {
    SerialOutput.outputToSerial(s,d);
  }

#endif

int PulseSensorPlayground::getPulseAmplitude(int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return -1; // out of range.
  }
  return Sensors[sensorIndex].getPulseAmplitude();
}

unsigned long PulseSensorPlayground::getLastBeatTime(int sensorIndex) {
  if (sensorIndex != constrain(sensorIndex, 0, SensorCount)) {
    return -1; // out of range.
  }
  return Sensors[sensorIndex].getLastBeatTime();
}

boolean PulseSensorPlayground::isPaused() {
	return Paused;
}

boolean PulseSensorPlayground::pause() {
  boolean result = true;
	if (USE_ARDUINO_INTERRUPTS){ // (UsingInterrupts) {
    if (!PulseSensorPlaygroundDisableInterrupt()) {
      Paused = false;
      result = false;
    }else{
			// DOING THIS HERE BECAUSE IT COULD GET CHOMPED IF WE DO IN resume() BELOW
			for(int i=0; i<SensorCount; i++){
				Sensors[i].resetVariables();
			}
			Paused = true;
		}
	}else{
		// do something more here?
		for(int i=0; i<SensorCount; i++){
			Sensors[i].resetVariables();
		}
		Paused = true;
	}
  return result;
}

boolean PulseSensorPlayground::resume() {
  boolean result = true;
	if (USE_ARDUINO_INTERRUPTS){ // (UsingInterrupts) {
    if (!PulseSensorPlaygroundEnableInterrupt()) {
      Paused = true;
      result = false;
    }else{
			Paused = false;
		}
	}else{
		// do something here?
		Paused = false;
	}
  return result;
}



// boolean PulseSensorPlayground::PulseSensorPlaygroundSetupInterrupt() {
//   boolean result = false;

//   #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)

//     // check to see if the Servo library is in use
//     #if defined Servo_h
//       // Initializes Timer2 to throw an interrupt every 2mS
//       // Interferes with PWM on pins 3 and 11
//       #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//         TCCR2A = 0x02;          // Disable PWM and go into CTC mode
//         TCCR2B = 0x05;          // don't force compare, 128 prescaler
//         #if F_CPU == 16000000L   // if using 16MHz crystal
//           OCR2A = 0XF9;         // set count to 249 for 2mS interrupt
//         #elif F_CPU == 8000000L // if using 8MHz crystal
//           OCR2A = 0X7C;         // set count to 124 for 2mS interrupt
//         #endif
//         TIMSK2 = 0x02;          // Enable OCR2A match interrupt DISABLE BY SETTING TO 0x00
//         ENABLE_PULSE_SENSOR_INTERRUPTS;
//         // #define _useTimer2
//         result = true;
//       #elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
//         TCCR3A = 0x02;          // Disable PWM and go into CTC mode
//         TCCR3B = 0x05;          // don't force compare, 128 prescaler
//         #if F_CPU == 16000000L   // if using 16MHz crystal
//           OCR3A = 0XF9;         // set count to 249 for 2mS interrupt
//         #elif F_CPU == 8000000L // if using 8MHz crystal
//           OCR3A = 0X7C;         // set count to 124 for 2mS interrupt
//         #endif
//         TIMSK3 = 0x02;          // Enable OCR2A match interrupt DISABLE BY SETTING TO 0x00
//         ENABLE_PULSE_SENSOR_INTERRUPTS;
//         // #define _useTimer2
//         result = true;
//       #endif
//     #else
//       // Initializes Timer1 to throw an interrupt every 2mS.
//       // Interferes with PWM on pins 9 and 10
//       TCCR1A = 0x00;            // Disable PWM and go into CTC mode
//       TCCR1C = 0x00;            // don't force compare
//       #if F_CPU == 16000000L    // if using 16MHz crystal
//         TCCR1B = 0x0C;          // prescaler 256
//         OCR1A = 0x007C;         // count to 124 for 2mS interrupt
//       #elif F_CPU == 8000000L   // if using 8MHz crystal
//         TCCR1B = 0x0B;          // prescaler = 64
//         OCR1A = 0x00F9;         // count to 249 for 2mS interrupt
//       #endif
//       TIMSK1 = 0x02;            // Enable OCR1A match interrupt DISABLE BY SETTING TO 0x00
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       result = true;
//     #endif
//   #endif

// #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)

//     // check to see if the Servo library is in use
//     #if defined Servo_h
//     // Initializes Timer1 to throw an interrupt every 2mS.
//     // Interferes with PWM on pins 9 and 10
//     TCCR1A = 0x00;            // Disable PWM and go into CTC mode
//     TCCR1C = 0x00;            // don't force compare
//     #if F_CPU == 16000000L    // if using 16MHz crystal
//       TCCR1B = 0x0C;          // prescaler 256
//       OCR1A = 0x007C;         // count to 124 for 2mS interrupt
//     #elif F_CPU == 8000000L   // if using 8MHz crystal
//       TCCR1B = 0x0B;          // prescaler = 64
//       OCR1A = 0x00F9;         // count to 249 for 2mS interrupt
//     #endif
//     TIMSK1 = 0x02;            // Enable OCR1A match interrupt
//     ENABLE_PULSE_SENSOR_INTERRUPTS;
//     result = true;

//     #else
//     // Initializes Timer2 to throw an interrupt every 2mS
//     // Interferes with PWM on pins 3 and 11
//       TCCR2A = 0x02;          // Disable PWM and go into CTC mode
//       TCCR2B = 0x05;          // don't force compare, 128 prescaler
//       #if F_CPU == 16000000L   // if using 16MHz crystal
//         OCR2A = 0XF9;         // set count to 249 for 2mS interrupt
//       #elif F_CPU == 8000000L // if using 8MHz crystal
//         OCR2A = 0X7C;         // set count to 124 for 2mS interrupt
//       #endif
//       TIMSK2 = 0x02;          // Enable OCR2A match interrupt
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       // #define _useTimer2
//       result = true;

//     #endif
//  #endif

//   #if defined(__AVR_ATtiny85__)
//     GTCCR = 0x00;     // Disable PWM, don't connect pins to events
//     OCR1A = 0x7D;     // Set top of count to 125. Timer match throws the interrupt
//     OCR1C = 0x7D;     // Set top of the count to 125. Timer match resets the counter
//     #if F_CPU == 16000000L
//       TCCR1 = 0x89;      // Clear Timer on Compare, Set Prescaler to 256
//     #elif F_CPU == 8000000L
//       TCCR1 = 0x88;      // Clear Timer on Compare, Set Prescaler to 128
//     #elif F_CPU == 1000000L
//       TCCR1 = 0x85      // Clear Timer on Compare, Set Prescaler to 16
//     #endif
//     bitSet(TIMSK,6);   // Enable interrupt on match between TCNT1 and OCR1A
//     ENABLE_PULSE_SENSOR_INTERRUPTS;
//     result = true;
//   #endif

//   #if defined(ARDUINO_SAMD_MKR1000)||defined(ARDUINO_SAMD_MKRZERO)||defined(ARDUINO_SAMD_ZERO)\
//   ||defined(ARDUINO_ARCH_SAMD)||defined(ARDUINO_ARCH_STM32)||defined(ARDUINO_STM32_STAR_OTTO)||defined(ARDUINO_NANO33BLE)

//   //  #error "Unsupported Board Selected! Try Using the example: PulseSensor_BPM_Alternative.ino"
//     result = false;      // unknown or unsupported platform.
//   #endif

// #if defined(ARDUINO_SAMD_MKR1000)|| defined(ARDUINO_SAMD_MKRZERO)|| defined(ARDUINO_SAMD_ZERO)\
// || defined(ARDUINO_ARCH_SAMD)|| defined(ARDUINO_ARCH_STM32)||(ARDUINO_STM32_STAR_OTTO)|| defined(ARDUINO_ARCH_NRF52)\
// || defined(ARDUINO_NANO33BLE)|| defined(ARDUINO_ARCH_RP2040)|| defined(ARDUINO_ARCH_ESP32)|| defined(ARDUINO_ARCH_MBED_NANO)\
// || defined(ARDUINO_ARCH_NRF52840)|| defined(ARDUINO_ARCH_RENESAS)|| defined(ARDUINO_ARCH_SAM)

//     result = true;
//   #endif



//     return result;
// }




// boolean PulseSensorPlayground::PulseSensorPlaygroundDisableInterrupt(){
//   boolean result = false;
// #if USE_ARDUINO_INTERRUPTS
//   #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
//     // check to see if the Servo library is in use
//     #if defined Servo_h
//       #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//         DISABLE_PULSE_SENSOR_INTERRUPTS;
//         TIMSK2 = 0x00;          // Disable OCR2A match interrupt
//         ENABLE_PULSE_SENSOR_INTERRUPTS;
//         result = true;
//       #elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
//         DISABLE_PULSE_SENSOR_INTERRUPTS;
//         TIMSK3 = 0x00;          // Disable OCR2A match interrupt
//         ENABLE_PULSE_SENSOR_INTERRUPTS;
//         result = true;
//       #endif
//     #else
//       DISABLE_PULSE_SENSOR_INTERRUPTS;
//       TIMSK1 = 0x00;            // Disable OCR1A match interrupt
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       result = true;
//     #endif
//   #endif

//   #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
//     // check to see if the Servo library is in use
//     #if defined Servo_h
//       DISABLE_PULSE_SENSOR_INTERRUPTS;
//       TIMSK1 = 0x00;            // Disable OCR1A match interrupt
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       result = true;
//     #else
//       DISABLE_PULSE_SENSOR_INTERRUPTS;
//       TIMSK3 = 0x00;          // Disable OCR2A match interrupt
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       result = true;
//     #endif
//   #endif

//   #if defined(__AVR_ATtiny85__)
//     DISABLE_PULSE_SENSOR_INTERRUPTS;
//     bitClear(TIMSK,6);   // Disable interrupt on match between TCNT1 and OCR1A
//     ENABLE_PULSE_SENSOR_INTERRUPTS;
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_ESP32)
//     timerAlarmDisable(sampleTimer);
//   #endif

//   #if defined(ARDUINO_ARCH_NRF52840)
//     sampleTimer.stopTimer();
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_RP2040)
//     sampleTimer.stopTimer();
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_RENESAS)
//     sampleTimer.stop();
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_SAM)
//     sampleTimer.stop();
//     result = true;
//   #endif

// #endif



// return result;      // unknown or unsupported platform.
// } // PulseSensorPlaygroundDisableInterrupt


// boolean PulseSensorPlayground::PulseSensorPlaygroundEnableInterrupt(){
//   boolean result = false;
// #if USE_ARDUINO_INTERRUPTS
//   #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__) // || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
//     // check to see if the Servo library is in use
//     #if defined Servo_h
//       #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//         DISABLE_PULSE_SENSOR_INTERRUPTS;
//         TIMSK2 = 0x02;          // Enable OCR2A match interrupt
//         ENABLE_PULSE_SENSOR_INTERRUPTS;
//         result = true;
//       #elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
//         DISABLE_PULSE_SENSOR_INTERRUPTS;
//         TIMSK3 = 0x02;          // Enable OCR2A match interrupt
//         ENABLE_PULSE_SENSOR_INTERRUPTS;
//         result = true;
//       #endif
//     #else
//       DISABLE_PULSE_SENSOR_INTERRUPTS;
//       TIMSK1 = 0x02;            // Enable OCR1A match interrupt
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       result = true;
//     #endif
//   #endif

//   #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
//     // check to see if the Servo library is in use
//     #if defined Servo_h
//       DISABLE_PULSE_SENSOR_INTERRUPTS;
//       TIMSK1 = 0x02;            // Enable OCR1A match interrupt
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       result = true;
//     #else
//       DISABLE_PULSE_SENSOR_INTERRUPTS;
//       TIMSK2 = 0x02;          // Enable OCR2A match interrupt
//       ENABLE_PULSE_SENSOR_INTERRUPTS;
//       result = true;
//     #endif
//   #endif

//   #if defined(__AVR_ATtiny85__)
//     DISABLE_PULSE_SENSOR_INTERRUPTS;
//     bitSet(TIMSK,6);   // Enable interrupt on match between TCNT1 and OCR1A
//     ENABLE_PULSE_SENSOR_INTERRUPTS;
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_ESP32)
//     timerAlarmEnable(sampleTimer);
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_NRF52840)
//     sampleTimer.restartTimer();
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_RP2040)
//     sampleTimer.restartTimer();
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_RENESAS)
//     sampleTimer.start();
//     result = true;
//   #endif

//   #if defined(ARDUINO_ARCH_SAM)
//     sampleTimer.start(2000);
//     result = true;
//   #endif

// #endif
    
// return result;      // unknown or unsupported platform.
// }


#if USE_SERIAL
  #if PULSE_SENSOR_MEMORY_USAGE
    void PulseSensorPlayground::printMemoryUsage() {
      char stack = 1;
      extern char *__data_start;
      extern char *__data_end;
      extern char *__bss_start;
      extern char *__bss_end;
      extern char *__heap_start;
      extern char *__heap_end;

      int data_size = (int)&__data_end - (int)&__data_start;
      int bss_size  = (int)&__bss_end - (int)&__data_end;
      int heap_end  = (int)&stack - (int)&__malloc_margin;
      int heap_size = heap_end - (int)&__bss_end;
      int stack_size  = RAMEND - (int)&stack + 1;
      int available = (RAMEND - (int)&__data_start + 1);
      available -=  data_size + bss_size + heap_size + stack_size;

      Stream *pOut = SerialOutput.getSerial();
      if (pOut) {
        pOut->print(F("data "));
        pOut->println(data_size);
        pOut->print(F("bss "));
        pOut->println(bss_size);
        pOut->print(F("heap "));
        pOut->println(heap_size);
        pOut->print(F("stack "));
        pOut->println(stack_size);
        pOut->print(F("total "));
        pOut->println(data_size + bss_size + heap_size + stack_size);
      }
    }
  #endif // PULSE_SENSOR_MEMORY_USAGE
#endif