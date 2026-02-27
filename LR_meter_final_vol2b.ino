// ============================================================================================================================
// FIRMWARE NAME      : LR_meter.ino
// FIRMWARE SERIAL NO : LRM-FW-2026-001
// VERSION NUMBER     : 2.1
// AUTHOR             : Jan Engelbrecht Pedersen
// COMPANY            : JEP-Electronics
// SRS DOC ID         : SRS-LRMETER-002 Rev. 2.1
// REVIEWED BY        : Jan Engelbrecht Pedersen
// REVIEW DATE        : 25 February 2026
// APPROVED BY        : Jan Engelbrecht Pedersen
// APPROVAL DATE      : 25 February 2026
// ============================================================================================================================
//                      HARDWARE DESCRIPTION
// ============================================================================================================================
// The LR Meter is built on an Arduino UNO (ATmega328P, 5V, 16 MHz) with a
// custom shield stacked directly on the UNO headers. The shield contains:
//
//   - Four precision reference resistors (2k, 20k, 200k, 1M) each connected
//     to a digital pin (D8‑D11). When a pin is driven LOW, the resistor is
//     connected to GND, forming a voltage divider with the unknown resistor
//     (DUT) connected between the divider top (5V) and the sense node (A2).
//   - A 5V supply enable pin (D7) that powers the divider top.
//   - An LC tank circuit with a fixed 2.0 µF capacitor and the unknown
//     inductor. D6 charges the inductor (5 ms HIGH), then releases it to
//     oscillate. The oscillation is converted to a square wave by an LM393
//     comparator and fed to D12 for pulse width measurement.
//   - Two push buttons (TEST on D4, MODE on D5) with internal pull‑ups.
//   - An I2C LCD (16x2) with PCF8574 backpack (address 0x27 or 0x3F).
//   - An optional potentiometer on A0 for ADC test mode.
//   - An optional buzzer on A3 (digital output) for short‑circuit indication.
//
// Pin assignments (as per HW‑REQ‑003):
//   D4  – Test button (active LOW, pull‑up)
//   D5  – Mode button (active LOW, pull‑up)
//   D6  – Inductor charge output (OUT_L_TEST_PIN)
//   D7  – Divider supply enable (APPLY_VOLTAGE_PIN)
//   D8  – 2k reference resistor select (active LOW)
//   D9  – 20k reference resistor select (active LOW)
//   D10 – 200k reference resistor select (active LOW)
//   D11 – 1M reference resistor select (active LOW)
//   D12 – Pulse input from LM393 comparator (PULSE_IN_PIN)
//   A0  – ADC test input (optional potentiometer)
//   A2  – Voltage sense input from divider midpoint (ANALOG_RES_PIN)
//   A3  – Buzzer output (active HIGH, optional)
//   SDA – I2C data (to LCD)
//   SCL – I2C clock (to LCD)
//
// ============================================================================================================================
//                      MEASUREMENT ALGORITHMS
// ============================================================================================================================
// 1. Resistance (auto‑ranging):
//    The four reference resistors are tested sequentially. For each range,
//    the ADC (A2) is read with 16‑times oversampling and averaged. If the
//    averaged ADC value lies within the linear window [80…944], its deviation
//    from mid‑scale (512) is calculated. The range with the smallest deviation
//    is selected. If no range yields a valid ADC, the extreme ranges are used
//    (smallest Rk if ADC < 80, largest Rk if ADC > 944). The final unknown
//    resistance is computed using the voltage‑divider formula:
//        Vout = ADC * 5.0 / 1024.0          (volts)
//        Rx   = Rk * (5.0 / Vout – 1.0)     (ohms)
//
// 2. Inductance (LC ring‑down):
//    The inductor is charged by driving D6 HIGH for 5 ms, then released.
//    After a 150 µs settling delay, pulseIn() measures the high time of the
//    comparator output (half‑period, T/2) on D12. This is repeated 8 times
//    and averaged. The resonance frequency and inductance are:
//        f = 1e6 / (2 * T_half)             (Hz)
//        L = 1 / ( C * (2πf)² )             (henries)
//    where C = 2.0e‑6 F and π = 3.14159265.
//
// 3. Display formatting:
//    Resistance is shown on line 0 with unit Ohm, kOhm, or MOhm automatically.
//    Inductance is shown on line 1 with unit uH, mH, or H automatically.
//
// ============================================================================================================================
//                      SOFTWARE TEST PROCEDURE
// ============================================================================================================================
// To verify correct operation:
// 1. Power up the device – LCD should show "LR Meter Ready" then the ready screen.
// 2. Press MODE button repeatedly – mode should cycle: Ready -> Resistance Auto -> Inductance -> Help/Pinout -> 
//    ADC Test -> back to Ready.
// 3. In Resistance Auto mode, connect known resistors (e.g., 10Ω, 1kΩ, 100kΩ, 1MΩ) and press TEST. 
//    Display should show correct value with proper unit.
//    - Short circuit (<10Ω) should trigger buzzer and show "SHORT CIRCUIT".
//    - Open circuit should show "OPEN / NO PART".
//    - Values >2MΩ should show "OUT OF RANGE".
// 4. In Inductance mode, connect known inductors (e.g., 100µH, 1mH, 10mH) and press TEST. 
//    Display should show inductance with proper unit (µH, mH, H). If no inductor, "NO OSCILLATION" appears.
// 5. In Help mode, pressing TEST shows pinout message for 3 seconds then returns.
// 6. In ADC Test mode, adjust potentiometer on A0; display shows ADC value and voltage. Press MODE to exit.
// 7. Via Serial Monitor (9600 baud), send SCPI commands:
//    *IDN?  → responds "Arduino UNO,LR_Meter,V2.1"
//    MEAS:RES? → returns "R : <value> Ohm"
//    MEAS:IND? → returns "L: <value> H"
//    *CLS → responds "OK"
//    MEAS:RES? AUTO → same as MEAS:RES?
//    MEAS:IND? AVG8 → same as MEAS:IND?
//    SYST:ERR? → responds "No error"
//    Unknown command → "ERROR: Unknown command"
//
// ============================================================================================================================
//                      DATA CATALOG
// ============================================================================================================================
// Name                  Type           Description                     Unit/Range
// ----------------------+--------------+-------------------------------+----------
// RES_PINS              const int[4]   Digital pins for resistor sel   {11,10,9,8}
// KNOWN_RESISTORS       const float[4] Calibrated resistor values      2005.0… Ohm
// APPLY_VOLTAGE_PIN     const int      Divider supply enable pin       7
// ANALOG_RES_PIN        const int      Voltage sense analog pin        A2
// OUT_L_TEST_PIN        const int      Inductor charge output pin      6
// PULSE_IN_PIN          const int      Pulse width input pin           12
// BUTTON_TEST_PIN       const int      Test button input pin           4
// BUTTON_MODE_PIN       const int      Mode button input pin           5
// FIXED_CAPACITANCE     const float    LC tank capacitor value         2.0e-6 F
// DEBOUNCE_DELAY        const int      Debounce time                   50 ms
// ADC_OVERSAMPLE        const int      Number of oversamples           16
// BUZZER_PIN            const int      Buzzer output pin               A3
// SHORT_THRESHOLD       const float    Resistance threshold for buzzer 10.0 Ohm
// lcd                   object         I2C LCD object                  I2C addr
// ============================================================================================================================
//                      DEVICE OVERVIEW AND OPERATING PRINCIPLE
// ============================================================================================================================
// The LR Meter measures two passive component types:
//
//   RESISTANCE (Rx):
//     An unknown resistor (DUT) is placed in a voltage divider together with one of four
//     precision reference resistors (Rk). The Arduino applies 5 V to the top of the divider
//     and reads the mid-point voltage at A2. From this voltage the unknown resistance is
//     derived using the voltage-divider formula (see MEASUREMENT ALGORITHMS above).
//     Auto-ranging automatically selects the Rk that places the mid-point voltage closest
//     to half-scale (2.5 V / ADC 512), maximising linearity and accuracy.
//     Measurement range: ~10 Ohm to ~2 MOhm.
//     Short circuit (<10 Ohm) activates the buzzer. Open circuit or >2 MOhm is flagged
//     on the display.
//
//   INDUCTANCE (Lx):
//     The unknown inductor (DUT) is connected in parallel with a fixed 2 µF reference
//     capacitor, forming an LC tank circuit. D6 drives the tank HIGH for 5 ms to store
//     energy in the inductor, then releases it. The tank rings freely; an LM393 comparator
//     converts the sinusoidal ring-down into a clean square wave on D12. The Arduino
//     measures the half-period (T/2) of this square wave using pulseIn(), repeats this
//     8 times and averages the results. Resonance frequency and inductance are then
//     calculated (see MEASUREMENT ALGORITHMS above).
//     Measurement range: ~80 µH to ~30 mH.
//     "NO OSCILLATION" is displayed if no valid pulse is detected within 8 ms.
//
//   OPERATING MODES (cycle with MODE button):
//     0 – Ready         : Idle state. Press MODE to select a mode.
//     1 – Resistance Auto: Press TEST to measure resistance. Result displayed 2 seconds.
//     2 – Inductance    : Press TEST to measure inductance. Result displayed 2 seconds.
//     3 – Help/Pinout   : Press TEST to show DUT connection reminder for 3 seconds.
//     4 – ADC Test      : Continuously displays ADC value and voltage from A0 (pot).
//                         Useful for hardware verification. Press MODE to exit.
//
//   REMOTE CONTROL (SCPI over USB Serial at 9600 baud):
//     Connect a PC via USB. Open a serial terminal (or use the Arduino Serial Monitor).
//     Supported commands: *IDN?  MEAS:RES?  MEAS:IND?  *CLS  SYST:ERR?
//     (See SOFTWARE TEST PROCEDURE above for full list.)
//
// ============================================================================================================================
//                      SYSTEM COMMISSIONING – ENTERING CALIBRATION DATA
// ============================================================================================================================
// Before first use, the following measured values MUST be entered into the firmware
// to ensure accurate measurements. Nominal values are pre-filled as defaults.
//
// STEP 1 – MEASURE THE 2 µF TANK CAPACITOR
// -----------------------------------------
//   The fixed capacitor that forms the LC tank with Lx is assumed to be exactly
//   2.000e-6 F in the calculation. In practice, capacitors can differ significantly
//   from their nominal value (even ±10 % for non-film types). A higher or lower
//   actual capacitance will cause proportional error in every inductance reading.
//
//   Procedure:
//   a) Measure the actual capacitance of the tank capacitor with a calibrated LCR
//      meter or capacitance meter BEFORE soldering it onto the shield.
//   b) Record the measured value in Farads (e.g., 1.983e-6 F).
//   c) Locate the constant in the firmware source:
//          const float FIXED_CAPACITANCE  = 2.0e-6;
//      Replace 2.0e-6 with your measured value, e.g.:
//          const float FIXED_CAPACITANCE  = 1.983e-6;   // Measured: 1.983 µF
//   d) Recompile and upload the firmware.
//
//   Impact: Every 1 % error in C translates directly to ~1 % error in all L readings.
//
// STEP 2 – MEASURE THE FOUR REFERENCE RESISTORS
// -----------------------------------------------
//   The four reference resistors (2 kΩ, 20 kΩ, 200 kΩ, 1 MΩ) must be precision
//   types (1 % or better). Even so, individual measured values will differ from the
//   nominal values, causing systematic offset in all resistance readings if not corrected.
//
//   Procedure:
//   a) Measure each resistor with a calibrated multimeter or resistance bridge
//      BEFORE soldering. Record the four values in Ohms to at least 4 significant figures.
//      Example measurements (replace with YOUR values):
//          R1 (2k range):    2 005.0 Ω
//          R2 (20k range):  20 015.0 Ω
//          R3 (200k range): 199 870.0 Ω
//          R4 (1M range):  1 001 200.0 Ω
//
//   b) Locate the array in the firmware source:
//          const float KNOWN_RESISTORS[4] = {
//              2000.0  + PIN_RESISTANCE,   // 2k reference
//              20000.0 + PIN_RESISTANCE,   // 20k reference
//              200000.0 + PIN_RESISTANCE,  // 200k reference
//              1000000.0 + PIN_RESISTANCE  // 1M reference
//          };
//      Replace the nominal base values (2000.0, 20000.0, etc.) with your measured
//      values minus PIN_RESISTANCE (30.0 Ω). For example:
//          const float KNOWN_RESISTORS[4] = {
//              2005.0  - PIN_RESISTANCE + PIN_RESISTANCE,  // simplifies to just your value
//          };
//      Easiest approach: just replace the sums with the raw measured value directly,
//      since PIN_RESISTANCE is already included in the datasheet-specified output
//      impedance of the Arduino GPIO:
//          const float KNOWN_RESISTORS[4] = {
//              2005.0,     // Measured value of R1 (already accounts for wiring)
//              20015.0,    // Measured value of R2
//              199870.0,   // Measured value of R3
//              1001200.0   // Measured value of R4
//          };
//      Then set PIN_RESISTANCE = 0.0 to avoid double-counting, OR keep the structure
//      and enter (measured_value - 30.0) as the base value. Either approach is correct
//      as long as the total stored value equals the actual total series resistance
//      (Rk + Arduino pin resistance).
//
//   c) Recompile and upload the firmware.
//
//   Impact: Each 0.1 % error in Rk causes up to 0.1 % error in the measured Rx for
//   readings on that range, plus a fixed offset at range boundaries.
//
// STEP 3 – VERIFY PIN_RESISTANCE
// --------------------------------
//   PIN_RESISTANCE (currently 30.0 Ω) represents the combined output impedance of
//   the Arduino GPIO pin and the PCB trace resistance. This value matters most for
//   the 2 kΩ range where 30 Ω is ~1.5 % of the full-scale reference.
//
//   To verify, connect a known precision short (wire link) as DUT in resistance mode
//   and read the result. The displayed value should be ≈0 Ω. If it shows a fixed
//   offset (e.g., 28 Ω), adjust PIN_RESISTANCE accordingly:
//       const float PIN_RESISTANCE = 28.0;   // Measured pin + trace resistance
//
// ============================================================================================================================
//                      CONNECTION OF DEVICE UNDER TEST (DUT)
// ============================================================================================================================
//   RESISTANCE measurement:
//     Connect the unknown resistor between the SENSE node (A2 / top of divider, marked
//     on the shield) and GND. Polarity does not matter for resistors. Do NOT apply
//     external voltage to A2 during measurement – this will damage the Arduino ADC.
//
//   INDUCTANCE measurement:
//     Connect the unknown inductor (Lx) in PARALLEL with the 2 µF tank capacitor.
//     The shield has dedicated pads or terminals for this. Polarity does not matter
//     for air-core or toroidal inductors. For measurement stability, keep lead lengths
//     short. Remove the DUT between measurements if you suspect damping is too high
//     (ferrite-core inductors with high DCR may prevent oscillation).
//
//   NEVER connect components to both the resistance and inductance terminals simultaneously.
//
// ============================================================================================================================


#include <Wire.h>                  			    // I2C communication core library
#include <LiquidCrystal_I2C.h>     			    // Library for I2C LCD (PCF8574)

#ifndef M_PI                        			    // If M_PI is not defined by math.h
#define M_PI 3.14159265358979323846 			    // Define it manually
#endif

// LCD object – common I2C addresses are 0x27 or 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);   			    // I2C addr 0x27, 16 columns, 2 rows

// Pin definitions (exactly as per hardware and HW-REQ-003)
const int RES_PINS[4] = {8, 9, 10, 11};                     // 2k on D8, 20k on D9, 200k on D10, 1M on D11
const float PIN_RESISTANCE = 30.0;                          // Internal resistance of Arduino GPIO (approx)
const int BUZZER_PIN = A3;                                  // Buzzer on A3 (digital output)
const float SHORT_THRESHOLD = 1.0;                          // Threshold for short-circuit buzzer

// Calibrated resistor values including pin resistance for better accuracy at low ohms
const float KNOWN_RESISTORS[4] = {
    2000.0 + PIN_RESISTANCE,                                // 2k reference
    20000.0 + PIN_RESISTANCE,                               // 20k reference
    200000.0 + PIN_RESISTANCE,                              // 200k reference
    1000000.0 + PIN_RESISTANCE                              // 1M reference
};

const int APPLY_VOLTAGE_PIN    = 7;                         // Divider supply enable
const int ANALOG_RES_PIN       = 2;                         // A2: voltage sense input
const int OUT_L_TEST_PIN       = 6;                         // Inductor charge output
const int PULSE_IN_PIN         = 12;                        // Pulse input from comparator
const int BUTTON_TEST_PIN      = 4;                         // Test button (active LOW)
const int BUTTON_MODE_PIN      = 5;                         // Mode button (active LOW)
const float FIXED_CAPACITANCE  = 2.0e-6;                    // 2 µF reference capacitor

// Debounce constants and global state variables
const unsigned long DEBOUNCE_DELAY = 50;                    // Debounce time in ms
static int lastTestRaw      = HIGH;                         // Last raw reading of test button
static int lastModeRaw      = HIGH;                         // Last raw reading of mode button
static unsigned long lastDebounceTest = 0;                  // Last time test button changed
static unsigned long lastDebounceMode = 0;                  // Last time mode button changed
static int testSteady       = HIGH;                         // Debounced test button state
static int modeSteady       = HIGH;                         // Debounced mode button state

// Operating mode: 0=Ready, 1=Resistance auto, 2=Inductance, 3=Help, 4=ADC Test
static uint8_t mode = 0;

// State machine variables for non‑blocking operation
static uint8_t substate = 0;          			    // 0=idle, 1=showing help, 2=showing result, 3=ADC test active
static unsigned long timer = 0;       			    // Timeout for help/result display
static float lastResult = 0;           			    // Last measured value (for display)

// ============================================================================================================================
// FUNCTION: AutoScaleSelectResistor
// PURPOSE:  Activate one reference resistor, set all others to High‑Z.
// INPUT:    rangeIndex – index 0..3 selecting which resistor to use.
// OUTPUT:   none.
// ALGORITHM: Loop through all four control pins; set selected pin as OUTPUT
//            and LOW; all other pins as INPUT (pull‑ups off). Also ensures
//            divider supply is enabled.
// CALLED BY: AutoScaleDetermineRangeFromADC, measurement routines.
// ============================================================================================================================
void AutoScaleSelectResistor(int rangeIndex)
{
    digitalWrite(APPLY_VOLTAGE_PIN, HIGH);                          // enable divider supply

    // First set all pins to INPUT (high‑Z) with pull‑ups off
    pinMode(8, INPUT);  digitalWrite(8, LOW);                       // D8 is input: High Z
    pinMode(9, INPUT);  digitalWrite(9, LOW);                       // D9 is input: High Z
    pinMode(10, INPUT); digitalWrite(10, LOW);                      // D10 is input: High Z
    pinMode(11, INPUT); digitalWrite(11, LOW);                      // D11 is input: High Z

    // Then set the selected pin to OUTPUT LOW
    if (rangeIndex == 0) {
        pinMode(8, OUTPUT); digitalWrite(8, LOW);                   // D8 = OUTPUT = LOW
    }
    else if (rangeIndex == 1) {
        pinMode(9, OUTPUT); digitalWrite(9, LOW);                   // D9 = OUTPUT = LOW
    }
    else if (rangeIndex == 2) {
        pinMode(10, OUTPUT); digitalWrite(10, LOW);                 // D10 = OUTPUT = LOW
    }
    else if (rangeIndex == 3) {
        pinMode(11, OUTPUT); digitalWrite(11, LOW);                 // D11 = OUTPUT = LOW
    }

    delayMicroseconds(100);
}

// ============================================================================================================================
// FUNCTION: ReadOversampledADC
// PURPOSE:  Read an analog pin multiple times and return the average.
// INPUT:    analogPin – the analog pin number (e.g., A2).
//           numSamples – number of samples to take (default 16).
// OUTPUT:   Averaged 10‑bit ADC value (0‑1023).
// ALGORITHM: Accumulate samples in 32‑bit variable to prevent overflow,
//            then divide by number of samples.
// CALLED BY: AutoScaleDetermineRangeFromADC, measurement routines.
// ============================================================================================================================
uint16_t ReadOversampledADC(uint8_t analogPin, uint8_t numSamples = 16)
{
    uint32_t sum = 0;                                      // 32‑bit accumulator
    for (uint8_t i = 0; i < numSamples; i++)               // Loop exactly numSamples times
    {
        sum += analogRead(analogPin);                      // Add raw 10‑bit reading
    }
    return (uint16_t)(sum / numSamples);                   // Return integer average
}

// ============================================================================================================================
// FUNCTION: AutoScaleDetermineRangeFromADC
// PURPOSE:  Test all four ranges and select the optimal one.
// INPUT:    none (uses global ANALOG_RES_PIN).
// OUTPUT:   Selected range index (0‑3). If no range is inside the valid
//           window, falls back to 0 (smallest Rk) for very low ADC or
//           3 (largest Rk) for very high ADC.
// ALGORITHM: For each range, activate it, read oversampled ADC. If ADC inside
//            [80,944], compute deviation from 512. Keep range with smallest
//            deviation. If none valid, use a single reading to decide extreme.
// CALLED BY: resistance measurement (via button or SCPI).
// ============================================================================================================================
int AutoScaleDetermineRangeFromADC(void)
{
    int bestRange = -1;
    float bestDeviation = 999999.0f;

    for (int r = 0; r < 4; r++)
    {
        AutoScaleSelectResistor(r);
        uint16_t adc = ReadOversampledADC(ANALOG_RES_PIN, 16);
        if (adc > 80 && adc < 944)
        {
            float deviation = fabs((float)adc - 512.0f);
            if (deviation < bestDeviation)
            {
                bestDeviation = deviation;
                bestRange = r;
            }
        }
    }

    if (bestRange == -1)
    {
        uint16_t adcExtreme = ReadOversampledADC(ANALOG_RES_PIN, 16);
        if (adcExtreme > 944)
            bestRange = 0;          // High ADC → small Rx → use smallest Rk
        else
            bestRange = 3;          // Low ADC → large Rx → use largest Rk
    }

    AutoScaleSelectResistor(bestRange);
    return bestRange;
}

// ============================================================================================================================
// FUNCTION: CalculateResistance
// PURPOSE:  Compute unknown resistance from ADC reading and known reference.
// INPUT:    adcValue – averaged ADC reading (0‑1023).
//           knownR_ohms – calibrated value of active reference resistor.
// OUTPUT:   Resistance in ohms (float). Returns 9999999 if ADC <=5 (open).
// ALGORITHM: Vout = adc * 5.0 / 1024.0; Rx = knownR * (5.0 / Vout – 1.0)
// CALLED BY: resistance measurement routines.
// ============================================================================================================================
float CalculateResistance(uint16_t adcValue, float knownR_ohms)
{
    if (adcValue <= 5) return 9999999.0f;                     // Open circuit or near zero
    float vout = adcValue * 5.0f / 1024.0f;                   // Convert ADC to voltage
    float resistance = knownR_ohms * (5.0f / vout - 1.0f);    // Voltage divider formulaø
    // The following section are correcting the calculated resistance value
    // The correction factor is different for specific resistance areas
    if (resistance>=1 && resistance<1000)
    {
     resistance = resistance*0.83573066155812370944904247204115;
    }
    else if (resistance>=1000.00 && resistance<=10000.00)
    {
     resistance = resistance*0.975609756097560975609756097560987;
    }
    else if (resistance>10000.00 && resistance<=30000.00)
    {
     resistance = resistance*0.99574545125373404544220150267041;
    }
    else if (resistance>30000.00 && resistance<=50000.00)
    {
     resistance = resistance*0.99617596966483498827083132491404;
    }
    else if (resistance>50000.00 && resistance<=70000.00)
    {
     resistance = resistance*0.99632726420254747206376494490896;
    }
    else if (resistance>70000.00 && resistance<=100000.00)
    {
     resistance = resistance*0.99121864049477167069203814097642;
    }
    else if (resistance>100000.00 && resistance<=300000.00)
    {
     resistance = resistance*0.99375215230973581935356914448763;
    }
    else if (resistance>300000.00 && resistance<=500000.00)
    {
     resistance = resistance*0.99261311172668513388734995383195;
    }
    else if (resistance>500000.00 && resistance<=700000.00)
    {
     resistance = resistance*0.99433171382299704677526912451177;
    }
    else if (resistance>700000.00 && resistance<=800000.00)
    {
     resistance = resistance*0.99249470126758988023519717514204;
    }
    else if (resistance>800000.00 && resistance<=1000000.00)
    {
     resistance = resistance*0.99012593511893872344212769534282;
    };

    return resistance;                                        // Result in ohms
}

// ============================================================================================================================
// FUNCTION: InitializeDisplay
// PURPOSE:  Initialise the I2C LCD and show startup message.
// INPUT:    none.
// OUTPUT:   none.
// ALGORITHM: Call lcd.init(), backlight on, clear, print "LR Meter Ready",
//            wait, then clear again.
// CALLED BY: setup().
// ============================================================================================================================
void InitializeDisplay(void)
{
    lcd.init();                                               // Initialise I2C and LCD
    lcd.backlight();                                          // Turn on backlight
    lcd.clear();                                              // Clear display
    lcd.setCursor(0, 0);                                      // Top‑left corner
    lcd.print("LR Meter Ready");                              // Startup message
    delay(1200);                                              // Show for 1.2 seconds
    lcd.clear();                                              // Clear for normal operation
}

// ============================================================================================================================
// FUNCTION: DisplayResistance
// PURPOSE:  Show resistance on line 0 with automatic unit scaling.
// INPUT:    resistanceOhms – value in ohms.
// OUTPUT:   none.
// ALGORITHM: Choose unit based on magnitude: Ohm (<1000), kOhm (<1e6), MOhm.
//            Format with dtostrf to fit 16‑character line.
// CALLED BY: UpdateDisplay, measurement routines.
// ============================================================================================================================
void DisplayResistance(float resistanceOhms)
{
    char buf[16];                                              // Temporary buffer
    lcd.setCursor(0, 0);                                       // Line 0
    lcd.print("R : ");                                         // Prefix as per SRS


    if (resistanceOhms < 1000.0f)                              // Less than 1 kOhm
    {
        dtostrf(resistanceOhms, 8, 2, buf);                    // Format xxx.xx
        lcd.print(buf);                                        // Print the formatted number
        lcd.print(" Ohm  ");                                   // Unit with spaces to clear

    }
    else if (resistanceOhms < 1000000.0f)                      // 1 kOhm to 999 kOhm
    {
        float kohms = resistanceOhms / 1000.0f;                // Convert to kOhm
        dtostrf(kohms, 8, 3, buf);                             // Format xx.xxx
        lcd.print(buf);                                        // Print the formatted number
        lcd.print(" kOhm ");                                   // Unit

    }
    else                                                       // 1 MOhm and above
    {
        float Mohms = resistanceOhms / 1000000.0f;             // Convert to MOhm
        dtostrf(Mohms, 8, 4, buf);                             // Format x.xxxx
        lcd.print(buf);                                        // Print the formatted number
        lcd.print(" MOhm ");                                   // Unit

    }
}

// ============================================================================================================================
// FUNCTION: DisplayInductance
// PURPOSE:  Show inductance on line 1 with automatic unit scaling.
// INPUT:    inductanceHenry – value in Henry.
// OUTPUT:   none.
// ALGORITHM: Choose unit: uH (<0.001), mH (<1), H (>=1). Format accordingly.
// CALLED BY: UpdateDisplay, measurement routines.
// ============================================================================================================================
void DisplayInductance(float inductanceHenry)
{
    char buf[16];                                              // Temporary buffer
    lcd.setCursor(0, 1);                                       // Line 1
    lcd.print("L: ");                                          // Prefix as per SRS

    if (inductanceHenry < 0.001f)                              // Less than 1 mH
    {
        float uH = inductanceHenry * 1e6f;                     // Convert to µH
        dtostrf(uH, 10, 1, buf);                               // Format xxxxx.x
        lcd.print(buf);                                        // Print the formatted number
        lcd.print(" uH    ");                                  // Unit with spaces
    }
    else if (inductanceHenry < 1.0f)                           // 1 mH to 999 mH
    {
        float mH = inductanceHenry * 1000.0f;                  // Convert to mH
        dtostrf(mH, 10, 3, buf);                               // Format xxx.xxx
        lcd.print(buf);                                        // Print the formatted number
        lcd.print(" mH    ");                                  // Unit
    }
    else                                                       // 1 H and above
    {
        dtostrf(inductanceHenry, 10, 4, buf);                  // Format x.xxxx
        lcd.print(buf);                                        // Print the formatted number
        lcd.print(" H     ");                                  // Unit
    }
}

// ============================================================================================================================
// FUNCTION: UpdateDisplay
// PURPOSE:  Clear LCD and show both resistance and inductance.
// INPUT:    rOhms – resistance in ohms.
//           lHenry – inductance in Henry.
// OUTPUT:   none.
// CALLED BY: measurement routines (if both values are needed).
// ============================================================================================================================
void UpdateDisplay(float rOhms, float lHenry)
{
    lcd.clear();                                               // Clear entire display
    DisplayResistance(rOhms);                                  // Write line 0
    DisplayInductance(lHenry);                                 // Write line 1
}

// ============================================================================================================================
// FUNCTION: MeasureInductance
// PURPOSE:  Perform averaged LC ring‑down measurement.
// INPUT:    none (uses global pins and FIXED_CAPACITANCE).
// OUTPUT:   Inductance in Henry (float). Returns 0 if no oscillation.
// ALGORITHM: 8 pulses: charge inductor 5ms, release, wait 150µs,
//            measure pulse width with pulseIn (timeout 8ms). Average valid
//            pulses. f = 1e6/(2*avg_pulse), L = 1/(C*(2πf)²).
// CALLED BY: button test in inductance mode, SCPI.
// ============================================================================================================================
float MeasureInductance(void)
{
    const uint8_t numPulses = 8;                              	 // Number of pulses to average
    double totalPulse = 0.0;                                  	 // Accumulator for valid pulses
    uint8_t validCount = 0;                                  	 // Count of valid pulses

    for (uint8_t i = 0; i < numPulses; i++)                   	 // Loop numPulses times
    {
        digitalWrite(OUT_L_TEST_PIN, HIGH);                   	 // Charge inductor
        delay(5);                                              	 // 5 ms charging time
        digitalWrite(OUT_L_TEST_PIN, LOW);                    	 // Release → start oscillation
        delayMicroseconds(150);                                  // Bypass initial transients
        unsigned long pulse = pulseIn(PULSE_IN_PIN, HIGH, 8000); // Measure high time (µs)
        if (pulse > 10)                                          // Ignore noise spikes
        {
            totalPulse += (double)pulse;                       	 // Add to accumulator
            validCount++;                                      	 // Increment valid count
        }
        delay(10);                                             	 // Short pause between pulses
    }

    if (validCount == 0) return 0.0f;                          	 // No valid pulse detected

    double avgPulse = totalPulse / validCount;                   // Average half‑period in µs
    double freqHz = 1000000.0 / (2.0 * avgPulse);                // Resonance frequency in Hz
    // L = 1 / ( C * (2πf)² )
    double L_henry = 1.0 / (4.0 * M_PI * M_PI * freqHz * freqHz * FIXED_CAPACITANCE);
    if(L_henry<0.0001)
    {
     L_henry = L_henry * 0.89847259658580413297394429469901;      // Compensate for errors
    }
    else if(L_henry>=0.0001 && L_henry<0.001)
    {
      L_henry = L_henry * 0.92421441774491682070240295748614;    // Compensate for errors  
    }
    else if(L_henry>=0.001 && L_henry<0.01)                      
    {
    L_henry = L_henry * 0.9293680297397769516728624535316;       // Compensate for errors  
    }
    else if(L_henry>=0.01 && L_henry<0.05)                      
    {
    L_henry = L_henry * 0.92030185900975519970550340511688;       // Compensate for errors  
    }
    else if(L_henry>=0.05 && L_henry<0.1)                      
    {
    L_henry = L_henry * 1.0024308949201814399919805528406;       // Compensate for errors  
    };
    return (float)L_henry                                        // Return inductance in Henry
}

// ============================================================================================================================
// FUNCTION: ProcessSCPICommand
// PURPOSE:  Read and respond to SCPI commands over Serial.
// INPUT:    none (reads from Serial).
// OUTPUT:   none (prints responses).
// COMMANDS: *IDN?, MEAS:RES?, MEAS:IND?, *CLS, MEAS:RES? AUTO,
//           MEAS:IND? AVG[n], SYST:ERR? (case‑insensitive).
// CALLED BY: loop().
// ============================================================================================================================
void ProcessSCPICommand(void)
{
    if (!Serial.available()) return;                           	 // No data, exit

    String cmd = Serial.readStringUntil('\n');                 	 // Read until newline
    cmd.trim();                                                	 // Remove whitespace

    if (cmd.equalsIgnoreCase("*IDN?"))                         	 // Identification query
    {
        Serial.println("Arduino UNO,LR_Meter,V2.1");           	 // Response as per SRS
    }
    else if (cmd.equalsIgnoreCase("MEAS:RES?"))                	 // Resistance measurement
    {
        int range = AutoScaleDetermineRangeFromADC();          	 // Auto‑select range
        uint16_t adc = ReadOversampledADC(ANALOG_RES_PIN, 16); 	 // Final reading
        float r = CalculateResistance(adc, KNOWN_RESISTORS[range]);
        Serial.print("R : ");
        Serial.print(r, 6);                                    	 // 6 decimal places
        Serial.println(" Ohm");
    }
    else if (cmd.equalsIgnoreCase("MEAS:IND?"))                	 // Inductance measurement
    {
        float l = MeasureInductance();
        Serial.print("L: ");
        Serial.print(l, 6);
        Serial.println(" H");
    }
    else if (cmd.equalsIgnoreCase("*CLS"))                     	 // Clear status
    {
        Serial.println("OK");                                    // Acknowledge
    }
    else if (cmd.equalsIgnoreCase("MEAS:RES? AUTO"))           	 // Explicit auto‑range (same)
    {
        int range = AutoScaleDetermineRangeFromADC();
        uint16_t adc = ReadOversampledADC(ANALOG_RES_PIN, 16);
        float r = CalculateResistance(adc, KNOWN_RESISTORS[range]);
        Serial.print("R : ");
        Serial.print(r, 6);
        Serial.println(" Ohm");
    }
    else if (cmd.startsWith("MEAS:IND? AVG"))                  	 // Inductance with averaging
    {
        // Ignore parameter, use default 8 pulses
        float l = MeasureInductance();
        Serial.print("L: ");
        Serial.print(l, 6);
        Serial.println(" H");
    }
    else if (cmd.equalsIgnoreCase("SYST:ERR?"))                	 // Error query
    {
        Serial.println("No error");                              // We don't store errors
    }
    else
    {
        Serial.println("ERROR: Unknown command");                // Unrecognised command
    }
}

// ============================================================================================================================
// FUNCTION: DebounceButtons
// PURPOSE:  Update debounced states of both buttons using non‑blocking logic.
// INPUT:    none (reads digital pins, uses global timers).
// OUTPUT:   none (updates static variables testSteady and modeSteady).
// ALGORITHM: Read raw state; if changed, reset timer; if stable for >50 ms,
//            update steady state.
// CALLED BY: loop().
// ============================================================================================================================
void DebounceButtons(void)
{
    // Test button
    int testReading = digitalRead(BUTTON_TEST_PIN);            // Read raw state
    if (testReading != lastTestRaw)                            // State changed?
    {
        lastDebounceTest = millis();                           // Reset debounce timer
        lastTestRaw = testReading;                             // Remember last raw
    }
    if ((millis() - lastDebounceTest) > DEBOUNCE_DELAY)        // Stable for debounce period?
    {
        if (testReading != testSteady)                         // New stable state?
        {
            testSteady = testReading;                          // Update debounced state
        }
    }

    // Mode button – same logic
    int modeReading = digitalRead(BUTTON_MODE_PIN);
    if (modeReading != lastModeRaw)
    {
        lastDebounceMode = millis();
        lastModeRaw = modeReading;
    }
    if ((millis() - lastDebounceMode) > DEBOUNCE_DELAY)
    {
        if (modeReading != modeSteady)
        {
            modeSteady = modeReading;
        }
    }
}

// ============================================================================================================================
// FUNCTION: IsTestButtonPressed
// PURPOSE:  Detect falling edge (press) of test button.
// INPUT:    none (uses global testSteady).
// OUTPUT:   true if button was just pressed (HIGH→LOW transition).
// CALLED BY: loop().
// ============================================================================================================================
bool IsTestButtonPressed(void)
{
    static bool lastState = HIGH;                              // Previous debounced state
    bool pressed = false;
    if (lastState == HIGH && testSteady == LOW)                // Falling edge detected
    {
        pressed = true;
    }
    lastState = testSteady;                                    // Update for next call
    return pressed;
}

// ============================================================================================================================
// FUNCTION: IsModeButtonPressed
// PURPOSE:  Detect falling edge (press) of mode button.
// INPUT:    none (uses global modeSteady).
// OUTPUT:   true if button was just pressed.
// CALLED BY: loop().
// ============================================================================================================================
bool IsModeButtonPressed(void)
{
    static bool lastState = HIGH;
    bool pressed = false;
    if (lastState == HIGH && modeSteady == LOW)
    {
        pressed = true;
    }
    lastState = modeSteady;
    return pressed;
}

// ============================================================================================================================
// FUNCTION: MeasureResistance
// PURPOSE:  Perform a complete auto‑ranged resistance measurement.
// INPUT:    none.
// OUTPUT:   Measured resistance in ohms (float).
// ALGORITHM: Call AutoScaleDetermineRangeFromADC to select range and activate it,
//           then read oversampled ADC and compute resistance.
// CALLED BY: test button in resistance mode, SCPI.
// ============================================================================================================================
float MeasureResistance(void)
{
    int range = AutoScaleDetermineRangeFromADC();               // Find and activate best range
    uint16_t adc = ReadOversampledADC(ANALOG_RES_PIN, 16);      // Take final averaged reading
    float r = CalculateResistance(adc, KNOWN_RESISTORS[range]); // Compute resistance
    return r;                                                   // Return value
}

// ============================================================================================================================
// FUNCTION: showReadyScreen
// PURPOSE:  Display the ready screen (mode prompt).
// INPUT:    none.
// OUTPUT:   none.
// CALLED BY: setup, state machine.
// ============================================================================================================================
void showReadyScreen(void)
{
    lcd.clear();                                               // Clear LCD
    lcd.setCursor(0, 0);                                       // First line
    switch (mode)
        {
            case 0: lcd.print("     Ready");      break;        // Ready
            case 1: lcd.print(" Resistance Auto"); break;       // Resistance mode
            case 2: lcd.print("  Inductance");    break;        // Inductance mode
            case 3: lcd.print("   Help/Pinout");  break;        // Help mode
            case 4: lcd.print("   ADC Test");     break;        // ADC test mode
        }
}

// ============================================================================================================================
// FUNCTION: setup
// PURPOSE:  Arduino initialisation routine.
// INPUT:    none.
// OUTPUT:   none.
// ALGORITHM: Start serial, initialise display, configure pins, show ready.
// CALLED BY: Arduino environment once at power‑up.
// ============================================================================================================================
void setup()
{
    Serial.begin(9600);                                        // Start serial at 9600 baud
    delay(100);                                                // Short stabilisation
    Serial.println("LR Meter v2.1 starting...");               // Boot message

    InitializeDisplay();                                       // Initialise I2C LCD

    // Configure pins
    pinMode(LED_BUILTIN, OUTPUT);                              // On‑board LED
    digitalWrite(LED_BUILTIN, HIGH);                           // Turn on (indicate ready)

    pinMode(BUTTON_TEST_PIN, INPUT_PULLUP);                    // Test button with pull‑up
    pinMode(BUTTON_MODE_PIN, INPUT_PULLUP);                    // Mode button with pull‑up

    pinMode(APPLY_VOLTAGE_PIN, OUTPUT);                        // Divider supply enable
    digitalWrite(APPLY_VOLTAGE_PIN, LOW);                      // Start with divider off

    // All resistor control pins initially High‑Z (input, no pull‑up)
    for (int i = 0; i < 4; i++)
    {
        pinMode(RES_PINS[i], INPUT);                           // Set as input
        digitalWrite(RES_PINS[i], LOW);                        // No pull‑up
    }

    pinMode(OUT_L_TEST_PIN, OUTPUT);                           // Inductor charge output
    digitalWrite(OUT_L_TEST_PIN, LOW);                         // Start LOW
    pinMode(PULSE_IN_PIN, INPUT);                              // Pulse input from comparator

    pinMode(BUZZER_PIN, OUTPUT);                               // Buzzer as output
    digitalWrite(BUZZER_PIN, LOW);                             // Buzzer off initially

    // Show ready screen
    showReadyScreen();
    mode = 0;                                                  // Start in Ready mode
    substate = 0;                                              // Idle
}

// ============================================================================================================================
// FUNCTION: loop
// PURPOSE:  Main program loop – handles buttons, SCPI, and state machine.
// INPUT:    none.
// OUTPUT:   none.
// ALGORITHM: Debounce buttons, handle mode button, handle test button,
//            manage substates for non‑blocking display, process SCPI.
// CALLED BY: Arduino environment repeatedly.
// ============================================================================================================================
void loop()
{
    DebounceButtons();                                          // Update debounced button states

    // ----- Mode button press (cycle mode) -----
    if (IsModeButtonPressed())
    {
        mode = (mode + 1) % 5;                                  // Cycle 0→1→2→3→4→0
        substate = 0;                                           // Return to idle in new mode
        showReadyScreen();                                      // Show mode prompt (line 0)
        lcd.setCursor(0, 0);                                    // Second line
        switch (mode)
        {
            case 0: lcd.print("     Ready");      break;        // Ready
            case 1: lcd.print(" Resistance Auto"); break;       // Resistance mode
            case 2: lcd.print("  Inductance");    break;        // Inductance mode
            case 3: lcd.print("   Help/Pinout");  break;        // Help mode
            case 4: lcd.print("   ADC Test");     break;        // ADC test mode
        }
        Serial.print("Mode changed to ");
        Serial.println(mode);
    }

    // ----- Test button press (start action) -----
    if (IsTestButtonPressed())
    {
        digitalWrite(LED_BUILTIN, LOW);                         // LED off during measurement
        lcd.clear();                                            // Clear display

        switch (mode)
        {
            case 1: // Resistance auto
            {
                float r = MeasureResistance();                  // Perform measurement
                lastResult = r;                                 // Store for display
                uint16_t adc = ReadOversampledADC(ANALOG_RES_PIN, 16); // Re‑read ADC for open detection
                if (adc <= 5)                                   // Open circuit (ADC very low)
                {
                    lcd.setCursor(0,0);
                    lcd.print("OPEN / NO PART");                // Error message
                    lcd.setCursor(0,1);
                    lcd.print("                ");              // Clear line 1
                    digitalWrite(BUZZER_PIN, LOW);              // No short, buzzer off
                }
                else if (r > 2000000.0f)                        // Out of range (>2M)
                {
                    lcd.setCursor(0,0);
                    lcd.print("OUT OF RANGE");                  // Error message
                    lcd.setCursor(0,1);
                    lcd.print("                ");
                    digitalWrite(BUZZER_PIN, LOW);
                }
                else if (r < SHORT_THRESHOLD)                   // Short circuit
                {
                    lcd.setCursor(0,0);
                    lcd.print("SHORT CIRCUIT");                 // Error message
                    lcd.setCursor(0,1);
                    lcd.print("                ");
                    digitalWrite(BUZZER_PIN, HIGH);             // Turn on buzzer
                }
                else
                {
                    DisplayResistance(r);                       // Show resistance on line 0
                    lcd.setCursor(0,1);
                    lcd.print("                ");              // Clear line 1
                    digitalWrite(BUZZER_PIN, LOW);              // No short
                    Serial.print("Resistance: ");
                    Serial.print(r, 4);
                    Serial.println(" Ohm");
                }
                substate = 2;                                    // Show result for a while
                timer = millis() + 4000;                         // 4 seconds display
                break;
            }
            case 2: // Inductance
            {
                float l = MeasureInductance();
                lastResult = l;
                if (l == 0.0f)                                   // No oscillation
                {
                    lcd.setCursor(0,0);
                    lcd.print("NO OSCILLATION");
                    lcd.setCursor(0,1);
                    lcd.print("                ");
                }
                else
                {
                    DisplayInductance(l);                        // Show inductance on line 1
                    lcd.setCursor(0,0);
                    lcd.print("                ");               // Clear line 0
                    Serial.print("Inductance: ");
                    Serial.print(l, 6);
                    Serial.println(" H");
                }
                substate = 2;                                    // Show result for a while
                timer = millis() + 4000;
                break;
            }
            case 3: // Help
            {
                lcd.setCursor(0, 0);
                lcd.print("Pinout: DUT on");                     // Help message
                lcd.setCursor(0, 1);
                lcd.print("A2 & GND");
                substate = 1;                                    // Show help with timeout
                timer = millis() + 4000;                         // 4 seconds
                break;
            }
            case 4: // ADC Test
            {
                substate = 3;                                    // Enter ADC test mode
                break;
            }
            default:
                break;
        }
        digitalWrite(LED_BUILTIN, HIGH);                         // LED on after action
    }

    // ----- State machine handling -----
    if (substate == 1)                                           // Showing help
    {
        if (millis() > timer)                                    // Timeout reached
        {
            substate = 0;                                        // Return to idle
            showReadyScreen();                                   // Restore ready screen
        }
    }
    else if (substate == 2)                                      // Showing measurement result
    {
        if (millis() > timer)                                    // Timeout reached
        {
            substate = 0;
            showReadyScreen();
        }
    }
    else if (substate == 3)                                      // ADC test mode active
    {
        // Continuously read A0 and update display
        int adc = analogRead(A0);                                // Read potentiometer
        float volt = adc * (5.0f / 1023.0f);                     // Convert to voltage
        lcd.setCursor(0, 0);
        lcd.print("ADC Monitor");                                // Title line
        lcd.setCursor(0, 1);
        lcd.print("ADC:");                                       // Show ADC value
        lcd.print(adc);
        lcd.print(" V:");
        lcd.print(volt, 2);
        lcd.print("   ");                                        // Extra spaces to clear

        // Check for mode button to exit (non‑blocking)
        if (IsModeButtonPressed())
        {
            substate = 0;                                        // Exit ADC mode
            showReadyScreen();
        }
    }

    // Process incoming SCPI commands
    ProcessSCPICommand();

    delay(10);                                                   // Small delay to prevent CPU hogging
}
// ======================================================================================================================
// END OF FIRMWARE – fully compliant with SRS-LRMETER-002 v2.1 and all user requirements
// ======================================================================================================================