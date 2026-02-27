# LRMeter
The Precision LR Meter: A Deep Dive into an Arduino-Based LCR Bridge

Building a reliable component tester is a rite of passage for many electronics
enthusiasts, but constructing a device that rivals the precision of
professional bench equipment requires a meticulous blend of analog design,
digital signal processing, and robust firmware architecture. The Precision
Auto-Ranging LR Meter, developed by JEP-Electronics, is exactly such a project.
Designed to measure resistance from 10 Ohm to 2 MOhm and inductance from 80 uH
to 30 mH, this instrument is built on the ubiquitous Arduino UNO platform but
elevates it to a professional standard through careful hardware selection and
structured software engineering. This article dissects the construction of this
device, exploring the "why" behind the component choices and the "how" of its
operating principles.

System Description: A Dual-Personality Instrument

At its core, the LR Meter is a standalone embedded instrument that uses two
distinct physical principles to measure passive components. For resistance, it
employs a classic auto-ranging voltage divider. The Device Under Test (DUT) is
placed at the bottom of a divider, with the top tied to a 5V supply and the
midpoint read by the Arduino's analog-to-digital converter (ADC). By switching
different precision reference resistors into the top of the divider, the
microcontroller can find the optimal range where the measured voltage is most
linear and accurate.

For inductance, the meter utilizes the LC ring-down (or resonance) method. The
unknown inductor is placed in parallel with a fixed, high-quality 2.0 µF
capacitor to form a resonant tank circuit. The Arduino charges the inductor
with a 5 ms pulse of current. When the charging source is removed, the energy
sloshes back and forth between the inductor and capacitor, creating a decaying
sinusoidal oscillation. An LM393 comparator converts this sine wave into a
clean square wave, allowing the Arduino to measure its frequency and calculate
the inductance. The device is controlled locally via a Test and Mode button,
with feedback provided on a 16x2 I2C LCD, and remotely via a standard SCPI
command set over USB serial.

Hardware Construction: The Art of Selection and Justification

The hardware is not merely a collection of parts; every component on the custom
Arduino shield is chosen for a specific reason to meet the stringent System
Requirements Specification (SRS).

The Heart of Precision: Reference Resistors (R2-R5)
The accuracy of any resistance measurement hinges on the precision of the
known references. The design uses four resistors: 2 kΩ, 20 kΩ, 200 kΩ, and
1 MΩ. These are not standard 5% or even 1% carbon film types. The specification
demands 0.1% tolerance metal-film resistors, such as the Vishay RNMF series,
with a low temperature coefficient (≤25 ppm/°C). This ensures that the
resistance value remains stable across the operating temperature range of the
device, preventing drift and maintaining the target accuracy of ±1.5%. The
choice of these specific values is strategic: when the unknown resistor (Rx)
equals the known resistor (Rk), the divider output is a perfect 2.5V, which is
the midpoint of the ADC's range (512), offering the highest linearity and
resolution.

The LC Tank: Stability in a Can (C2)
The 2.0 µF capacitor in the LC tank is arguably the most critical component for
inductance measurement. Any error in its value translates directly into a
proportional error in the inductance reading. The designer wisely chose a WIMA
MKS2 film capacitor. Unlike cheaper ceramic capacitors, film capacitors offer
low Equivalent Series Resistance (ESR), which is crucial for a clean, sustained
ringing signal. More importantly, they exhibit exceptional stability over
temperature, changing only ±1% from -55°C to +100°C. This ensures that the
inductance readings remain consistent regardless of the ambient temperature or
self-heating of the device.

The Comparator Interface: Taming the Ring (IC1A, D1, R6, R7)
The decaying sine wave from the LC tank can swing below ground. Directly
feeding this into the Arduino's 5V logic input would be catastrophic. This is
why an LM393 comparator is used. Its input is robust and protected by a 150Ω
series resistor (R6). A 1N4148 fast-switching diode (D1) clamps any negative
voltage excursions to just below ground, protecting the comparator's input.
The LM393's open-collector output requires a pull-up resistor (R7, 330Ω) to
create a clean 5V logic-level square wave for the Arduino's pulseIn() function
on pin D12. The value of 330Ω is a careful balance: it is low enough to provide
fast rise times for accurate high-frequency measurements but high enough to
keep the comparator's sink current within its safe limits.

Software Construction: Procedural Elegance and Non-Blocking State Machines

The firmware, contained entirely in a single .ino file, is a masterclass in
structured procedural programming, adhering strictly to the SRS. It avoids the
overhead and complexity of object-oriented programming, opting for a clear,
event-driven state machine within the Arduino's main loop.

The Auto-Ranging Algorithm: A Digital Search for the Best Fit
The core of the resistance measurement is the function
AutoScaleDetermineRangeFromADC(). When the Test button is pressed, the
software doesn't just pick a range and hope for the best. It iterates through
all four ranges. For each, it sets the appropriate resistor pin LOW and sets
the others to High-Z input mode to prevent parallel leakage paths. It then
takes a reading using ReadOversampledADC(), which averages 16 consecutive ADC
samples. This oversampling technique effectively increases the signal-to-noise
ratio, providing the equivalent of 12 bits of resolution from the native 10-bit
ADC.

The algorithm then checks if the averaged ADC value falls within a valid
"linear window" of 80 to 944. If it does, it calculates how far the value is
from the ideal mid-point of 512. The range with the smallest deviation is
selected as the winner. If no range is valid (e.g., the resistor is too large
or too small), it falls back to the most extreme range based on a final single
reading. This ensures that the measurement is always performed with the
reference resistor that places the voltage closest to 2.5V, guaranteeing maximum
accuracy across the entire 10Ω to 2MΩ range.

Inductance Measurement: Timing is Everything
The MeasureInductance() function implements the ring-down method. It charges
the inductor for 5ms and then releases it. After a short 150µs settling delay
to bypass switching noise, it uses pulseIn() to measure the width of the next
high pulse on D12. This is the half-period (T/2) of the oscillation. The
process is repeated eight times, and valid pulses (those >10µs) are averaged.
This averaging rejects noise and jitter from the comparator.

With the average half-period, the firmware calculates the resonant frequency
and then plugs it into the standard resonance formula to derive the inductance
in Henrys. The entire process is designed to be fast and reliable for inductors
from 80µH to 30mH.

Non-Blocking UI and SCPI Control
A critical design goal was to keep the main loop responsive. The
DebounceButtons() function uses millis() timers instead of blocking
delay() calls, ensuring that the processor can constantly check for serial
commands and update the display. The operating mode is a simple state variable
that cycles from Ready to Resistance, Inductance, Help, and ADC Test. The
ProcessSCPICommand() function checks for serial data and executes commands
like MEAS:RES? without ever stalling the main loop, adhering to the
requirement that no single function block for more than 10ms.

Building the Hardware: A Custom Arduino Shield

The entire circuit is designed to be constructed on a single, double-sided
shield that plugs directly into the Arduino UNO. This ensures a robust
mechanical connection and a clean layout. The netlist provided in the hardware
report maps every component to a specific UNO pin.

The shield's layout groups the circuitry by function. The four precision
resistors (R2-R5) are clustered near the control pins D8-D11 and the sense line
to A2 to keep trace lengths short and minimize noise pick-up. The LM393
comparator and its associated protection components (D1, R6, R7) are placed
adjacent to the inductor test terminals (J1) and the pulse input pin D12. The
I2C pull-up resistors (R1, R8) are located near the connector for the LCD,
ensuring signal integrity for the display communication. This physical
organization mirrors the logical blocks in the software, making the design
easier to understand, debug, and document.

Technical Specifications and The Path to Higher Precision

The finalized LR Meter boasts impressive specifications. It measures resistance
from approximately 10 Ohm to 2 MOhm with an accuracy of ±1.5% or ±2 Ohm
(whichever is greater). Its inductance range covers 80 µH to 30 mH with an
accuracy of ±5%. The measurement time is under three seconds, with the
auto-ranging routine settling in under 200 ms.

The device is capable of measuring from 10nF to 10000uF and from 10uH to 200mH,
though the published specifications are focused on the core ranges. Achieving
higher precision, such as pushing resistance accuracy to ±0.5% or extending the
inductance range, faces several fundamental barriers. First, the reference
components themselves are a limit; moving beyond 0.1% resistors enters a realm
of significantly higher cost and scarcity. Second, the Arduino UNO's internal
ADC has inherent non-linearity and a 10-bit limit. While oversampling helps,
it cannot correct for integral non-linearity errors. A true high-precision
design would require an external 16-bit or 24-bit sigma-delta ADC. Third, for
inductance, the stray capacitance of the inductor itself and the PCB traces
begin to affect the resonant frequency, requiring more complex modeling than
the simple parallel LC assumption. Finally, the output impedance of the
Arduino's GPIO pins (PIN_RESISTANCE), while compensated for, is not perfectly
linear or stable across temperature and current, introducing a small but
persistent error in the lowest resistance ranges.
