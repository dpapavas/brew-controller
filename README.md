<!-- -*- coding: utf-8 -*- -->

# Brew controller

![Title](./doc/title.jpeg?raw=true)

This is a controller module designed as an add-on to the Gaggia Classic espresso
machine, although from a functional perspective it can be used with any "dumb"
espresso machine, i.e. any machine with a simple thermostat for boiler
temperature control and a simple on/off switch for pump control.  It can control
the brew process in both an automatic mode, following a programmed brew profile
based on flow, pressure and/or yield weight, or in manual mode, allowing direct,
real-time control of flow, pressure, or pump power.

## Design

The controller consists of the following parts:

* A pressure sensor, attached to the exit of the boiler, just before the group
  head.

* A flow sensor, attached to the pump inlet.

* A load cell, mounted below the drip tray, weighing the yielded coffee.

* A controller board, consisting of a pair of TRIAC switches, controlling power
  to the boiler and pump and a microcontroller, interfacing the sensors and
  controlling these switches.

* An enclosure and other assorted accessories, used to mount all the above to
  the espresso machine.

The design is freely available, but anyone considering their own build is urged
to read the discussion below, as there are some issues that will need to be
addressed.

## Operation

So, is this useful?  Well, yes, it is. All features, while arguably not
necessary, do offer additional useful control over the brewing process.
Temperature control improves temperature stability and control, allowing the
brewing to be adapted to the coffee at hand and, more importantly, improving the
consistency of the result across multiple shots.  Pump power control, makes
preinfusion possible and, in combination with pressure and flow measurement, it
also makes control of these parameters possible.  Pressure control is useful to
avoid channeling, while flow control can be useful, both to control preinfusion
where pressure has not yet developed and to give some control over brew time.
Weighing the yield allows automatic control of the brew ratio, as well as more
sensible variation of other parameters during the brew process (for instance the
pressure can be tapered off during the final third of the shot in terms of the
yield mass, instead of time).

The following plot is from data recorded by the controller during a programmed
shot.

![Shot graph](./doc/graph.svg?raw=true)

The machine is already warmed up, as can be seen by the stable temperature (1)
and the small (less than 5% of the total) power needed at the boiler to maintain
it (2).  At about 12s the brew switch is pressed and the shot starts with a
preinfusion phase (3).  The pump is engaged at low power to push water through
the grounds at 3 ml/s.  As this happens, the temperature starts to drop, as cold
water enters the boiler, and power to the boiler is increased in order to
compensate.  Pressure gradually develops as the grounds expand and at 4 bars the
preinfusion phase is terminated (4).  Power to the pump is cut off, flow
vanishes and yield mass starts to rise as coffee starts to drip into the cup.

A blooming phase follows for the next 30s (5), where nothing much happens, apart
from temperature stabilization, a gradual drop-off in pressure and slight
increase in yield, as coffee leaks into the cup.  At 54s, the pump is engaged
once more, at higher power, and pressure is gradually ramped up to 9 bars (6).
Note the high initial flow, as the brew chamber is refilled, followed by a sharp
fall-off as the puck is compressed and pressure develops leading to oscillations
as the PID struggles somewhat to adapt to the complex dynamics.  This
necessitates both a gradual ramp-up in pressure (here from 0 to 9 bars in 5s) as
well as an additional wait time of 5s at 9 bars, while the flow settles.  After
that (7) the brew proceeds at constant flow rate, keeping the flow steady at the
1.7 ml/s it settled at.

In this particular shot, this strategy fails partially.  Maintaining the flow at
the instantaneous value measured at the end of the settling wait interval, leads
to a higher than desired flow and a pressure increase to a bit above 10 bars.
This can be corrected manually, if desired, either by reducing the flow to reduce
the developed pressure, or by reducing the pressure immediately.  No such action
has been taken here and the shot proceeds at a constant 10 bars and about 1.7
ml/s.  Note the disparity in volume and mass, the former of which is calculated
from the flow sensor, while the latter is measured via the load cell.  Some of
the difference is due to the water retained by the grounds, although most of
that has been accounted for, when the volume was reset at the end of the
preinfusion phase (4).  The rest of the difference is owed to inaccuracies in
the flow sensor, which accumulate as its signal is integrated to get the volume.

Finally the brew process is stopped at about 89s, with a total yield of 50g (8)
and pressure and flow drop to zero.  The sudden uptick in mass, is due to the
water remaining in the chamber being evacuated to the drip tray, via the
three-way solenoid valve.

Note that the steam related functionality of the machine is unaltered.  The
steam switch still heats up the boiler through the separate high temperature
thermostat mounted on the top of the boiler.

## Build

Although the design works quite well and is, at the time of this writing,
seemingly bug-free and refined enough for efficient and hassle-free daily use,
it does unfortunately suffer from a few design flaws that make it unsuitable for
anyone who might be considering building it.

For one, it was designed by someone that is not an electrical engineer.
Although as much care as possible was taken to ensure a good design, including
simulations and proper calculations where appropriate, the result does have a
few shortcomings which will probably need to be addressed:

* It is based on a Teensy 3.1/3.2 microcontroller, which is sadly no longer in
  production.  Any builds will therefore need to be ported to a more modern
  controller.  This problem is further compounded by the fact that the source
  code is "bare-metal", i.e. no hardware abstraction libraries are used, so that
  porting to a different controller is a non-trivial task.  Note though, that
  teasing out the application specific-code, i.e. the code implementing PID
  control, sensor signal reading and conditioning and rewriting the
  machine-specific parts, such as SPI, I2C, GPIO, timing and interrupt handling
  using a high-level library for the target controller, should not be a
  particularly hard task, given some experience.

* The coffee machine's brew switch, i.e. the one turning on the pump and
  starting the brewing process, is read through a GPIO input that is grounded
  via the switch.  The current design depends on the internal pull-up for this,
  which is too weak and hence prone to EMI.  This should probably be addressed in
  a potential board redesign, by including a stronger external pull-up resistor.

* In the current design, brewing is still controlled by the machine's brew
  switch, which is responsible for turning power to the pump on and off.  While
  this power can then be further controlled via the TRIAC, the three-way
  solenoid valve that is controlled by the same switch cannot.  Relegating this
  task to a relay, controlled by an additional GPIO output could therfore be
  useful, as it would allow automatically stopping the brew process, for
  instance when the desired yield has been achieved.  While this can be done
  now, one still needs to turn the switch off, to turn off the solenoid.  Doing
  this after the pump has been cut off, results in low pressure in the brew
  chamber and hence incomplete evacuation of the remaining brew water,
  nullifying the solenoid valve.

Due to the above, it is not expected that this design will be built by others.
The following discussion will therefore be limited to a general outline of the
build process.  Anyone intending to attempt a build is welcome to open an issue
for further clarifications.

### Parts

The parts needed to populate the board are the following:

|Designator|Description|
|----------|-----------|
|R3,R4|22KΩ, 1W through-hole resistor|
|R5|10KΩ, 1/4W through-hole resistor|
|U5|LD1117, 3.3V, TO-220-3 voltage regulator|
|C4,C2,C5|100nF through-hole ceramic resistor|
|D1|DB104, 400V, 1A bridge rectifier|
|R7,R6|360Ω, 1W through-hole resistor|
|Q1,Q2|2N7000, 0.2A, TO-92 N-Channel MOSFET|
|R8|680Ω, 1W through-hole resistor|
|U3,U2|MOC3021M, 400V optocoupler|
|Q3,Q4|BTA16-800CW 800V, TO-220-3 TRIAC|
|U4|Adafruit MAX31865 Module, or compatible|
|J4|JST XH 2-pin vertical connector|
|R2,R1|82Ω, 1/4W through-hole resistor|
|J2|Pin header, 2.54mm|
|J3|Pin header, 2.54mm|
|J5|Pin header, 2.54mm|
|U1|EL817 or PC817 optocoupler|
|C3|10u radial through-hole capacitor (low profile)|
|U6|Teensy 3.1 or 3.2|
|C1|100n, X2 through-hole capacitor|
|J1|5.08mm terminal block|

The following additional parts are also necessary:

* Heat sinks for the TRIACs.  Extruded aluminum heat sinks of size 46x20x10mm
  fit the enclosure and provide adequate cooling when properly mounted with heat
  sink compound.

* XP POWER ECL10US05-T 5V, 10W AC/DC power supply.

* Waveshare 1.5" RGB OLED Module, based on the SSD1351 controller.

* Waveshare rotation sensor, or similar push-button incremental rotary encoder.

* A PT100 temperature probe, preferably 3-wire.

* XDB-401 pressure sensor, in SS316L steel, 0-12 bar range, G1/8" thread, I2C
  interface.

* Shades Of Coffee Brew pressure gauge adaptor kit, or similar, along with a
  G1/8" female-to-female adaptor, to connect the pressure sensor to the boiler
  output.

* 3.3V hall-effect flow sensor.  The SEN-HZ06D can be used and has a suitably
  low range of 0.05-1.1 L/min.

* Adafruit NAU7802 24-Bit ADC module, or compatible.

* 1Kg 75x12.6x12.6mm load cell.  Mount holes should be M4, 10mm apart to fit the
  base and top plate.

* Shades of coffee slim drip tray.
 
* M3xM5x6mm threaded inserts for the enclosure.

* At least 1mm² silicon wire in various colors for high voltage/current wiring.

* Lower gauge silicon wire for the connection to the the espresso machine's brew
  switch.  Ideally of low enough gauge to allow crimping and fitting a
  Dupont-style terminal.

Finally, some parts need to be 3D printed.  See the [Things](#things) section
below.

# Assembly

Drill a total of six holes on the side of the espresso machine.  The 3D printed
mounting gasket can serve as a stencil to mark the locations and sizes of the
holes.  The four smaller peripheral holes serve to mount the enclosure.  Its
mounting bosses, where the threaded inserts are installed, should be able to
pass through those holes, barely protruding from the other side with the gasket
installed.  The two larger central holes serve to pass wiring into the espresso
machine.  One should be used for high voltage wiring and one for low voltage
wiring, to keep them separated.  Note that it is not absolutely necessary to
fully disassemble the espresso machine in order to drill the holes, although
that is the safest option.  With care, the operation can be carried out with
only the boiler screws and a few of its wires removed and the boiler simply held
out of the way, partially out of the machine.

Install the threaded inserts, assemble the controller board, including the
Teensy and TRIAC heat sinks and mount all boards onto the enclosure.  If the
supplied wiring brackets are used, it is recommended to glue the nuts on their
recesses, as they're otherwise held very loosely and can be a major source of
pain during assembly and repairs.

## Internal wiring

With the boards in place, the following internal wiring connections should be made:

* **Display module**: Attach the supplied 7-pin ribbon to the upper half
    (i.e. the part closest to the Teensy, marked `3V3`, `GND`, ..., `RST`) of
    the `J3` connector, noting the insertion orientation.

* **Rotary encoder**: Attach the supplied 5-pin ribbon to the `J2` connector on
    the board, noting the insertion orientation.

* **Supply**: Attach a suitable JST terminated cable from the output of the PSU
    board to the `J5` connector.  Although the connector is keyed, make sure the
    connector on the side of the controller board hasn't been soldered with
    incorrect orientation.  Also attach a 2-wire cable, terminated on one side
    with a suitable 3-pin JST connector to the input of the PSU board.  The bare
    wires on the other side, should be screwed into the terminal block plugged
    into the `J1` connector, in the pins marked `L` and `N`.  The polarity is
    not important.

After these steps, the assembled enclosure should look like this:

![Controller assembly](./doc/controller_assembly.png?raw=true)

## Hardware assembly

Attach all external wiring (see below) to the appropriate controller connectors
and pass the wires through the two chassis holes, keeping high and low voltage
wires separated.  Place the enclosure so that the threaded bosses pass through
the holes drilled in the chassis and fasten it using the supplied countersunk
washers.  The lower washers can be hard to install, but it can be accomplished
using a short magnetic screw driver, if the boiler screws are removed and the
boiler moved out of the way, to gain access through the group head hole in the
chassis.  If the pressure sensor is used, its mount bracket should be used in
place of the washer for the upper hole closest to the middle of the machine.

Once the controller is fastened, the lower boiler thermostat should be removed
and replaced with the PT100 sensor.  The pressure sensor should also be
connected to the pressure gauge kit and mounted in the bracket, secured with a
zip tie passed through the slot in the back of the bracket.  The flow sensor
should be installed in line with the pressure inlet hose.

Two M4x12mm countersunk screws should be installed together with the supplied
"locating pins" in the holes at the top of the load cell base.  These form
locating bosses, mating with the holes at the bottom of the machine's chassis to
keep the base in place.  The load cell can be installed in either side, but the
right hand side is more convenient, as it facilitates the connections between
the controller module and both the load cell and the I2C bus.

The partially assembled scale looks like this:

![Partially assembled scale](./doc/scale_disassembled.png?raw=true)

The installed scale, can be seen [here](./doc/scale_installed.png).

## External wiring

The instructions below will be brief, intended more as a general strategy for
making the connections to the machine, rather than detailed instructions.  The
photograph should hopefully give a clearify the setup further, but please study
the wiring diagram first and take every care must be taken to ensure the proper
connections are made in a safe manner.

The proposed connections are for the EU model with the ECO timer module.  The
ECO timer will need to be bypassed, so as to gain access to a pair of contacts
at the brew switch for the controller sense connection. A simple wire with two
spade connectors is needed, but kits with installation instructions are also
sold online.

Note that live and neutral are treated loosely here and no care needs to or has
been taken to ensure that they correspond to the live and neutral at the
plug. They will therefore be indicated by "neutral" and "live" below.

### High voltage

* **"Neutral" input**: The "neutral" connection that powers the controller can
    be taken from the brew switch.  This requires a wire with three connectors
    on one end: one male spade connector plugging into the machine's "neutral"
    wire, after removing it from the brew switch, one female spade connector to
    restore the connection to the brew switch and another female spade
    connector, going back to the pump.

    The pump already has a connection for "neutral", but it passes through the
    brew switch, which creates a problem.  As the pump is switched off by the
    switch, back-EMF is created at the pump solenoid, leading to large voltage
    spikes and noise.  This can create all sorts of problems.  For instance it
    can pull the programming pin of the controller low and reboot it into
    programming mode, but it can also create I2C transmission problems, etc.
    Having an unswitched "neutral" connection allows the pump to be switched off
    via the TRIAC, which always ensures switching happens at zero current, hence
    no back-EMF or noise spikes.

    The other, unterminated end should be screwed into the controller's terminal
    block, at the place marked `N`, along with one of the small wires going to
    the PSU input.

* **"Live" input**: The live input can be taken from the lead going to the
    removed thermostat.  The other end should be screwed into to the
    controller's terminal block, at the place marked `L`, along with one of the
    other of the two small wires going to the PSU input.

* **Temperature control**: The other lead removed from the thermostat should be
    connected to the controller's terminal block, at the place marked `HEAT`.
    In this way, the controller's TRIAC switch essenatially assumes the place of
    the OEM thermostat switch.

* **Pump control**: The other terminal of the pump should be screwed into the
    controller's terminal block, at the place marked `PUMP`.  Note that the
    thermal fuse mounted on the pump is now by-passed.  It can be retained
    either by cutting and reusing the existing part, or by using a spare part,
    connected in-line with the wire going to the controller.

* **Solenoid snubber**: The three-way solenoid valve can generate noise, both
    due to the inrush current at turn-on, as well as due to the back-EMF induced
    voltage spike at turn-off.  The latter is more pronounced and can generate
    all sorts of havoc, such as resetting the controller, pulling encoder lines
    low generating phantom button presses, etc.  A simple (0.1 μF, 100 Ω, 1/2W,
    600 VAC) RC snubber should therefore be installed accross its line and
    neutral terminals to alleviate such problems.  Snubbers such as this can be
    readily found as a single discrete part, which, together with a few
    terminals and short wiring, can be attached to the valve without
    modifications to the OEM wiring.

### Low voltage

* **I2C bus**: A 4 core shielded cable should be used for the I2C bus.  At one
    end the following connections need to be made at the controller, at connetor
    `J3`:
        - `GND` to I2C ground
        - `PC2` to I2C +3.3V
        - `PB2` to I2C SCL
        - `PB3` to I2C SDA

    At the other end the cable should be connected to the pressure sensor and
    the load cell sensor board.  A thin enough cable can be routed through a gap
    in the mahine's chassis and behind and below the water tank.  The I2C wiring
    should be kept as short as possible, leaving enough slack to pull out the
    load cell frame when required.  The shield wire should be attached to the
    ground at both ends.

* **Flow sensor**: The wires from the flow sensor should be connected to the
    controller board, at connetor `J3`, as follows:
        - `PD5` to sensor +3.3V
        - `PD6` to sensor ground
        - `PC1` to sensour output

* **Temperature sensor**: The PT100 wires should be connected to the MAX31865
    module.  Consult its datasheet for the proper connections, as well as solder
    jumper configuration.

* **Brew switch sense**: The brew switch contacts left unused by the ECO timer
    by-pass should be connected to the controller, at the connetor `J5`.  One of
    the connectors pins is a ground pin and this should also be connected to the
    machine's chassis ground.  This connection serves to reference the
    controller's ground to the mains ground, preventing ground loops when the
    USB connector is attached to a mains-powered desktop computer.  It can be
    conveniently established with a female spade terminal, at the unused male
    spade forming part of the thermal fuse bracket screwed to the top of the
    boiler.

    A 100nF capacitor should also be soldererd between these two wires.  This
    serves to prevent votlage dips induced due to the large voltage (and hence
    current) differentials as the TRIAC switches the boiler on mid-cycle.

With all connections made, the machine's wiring should look similar to this:

![Sensors and wiring](./doc/sensors_and_wiring.png?raw=true)

# Software

To compile the firmware, you'll need the `gcc-arm-none-eabi` toolchain and the
Teensly CLI loader. With everything installed, compiling and loading the
firmware can be accomplished with:

```bash
$ cd src/
$ make all
$ make install
```

The firmware boots up in "automatic" mode; pressing the brew switch at this
point will execute the loaded program.  Pressing the encoder wheel button cycles
through the various manual modes, allowing manual control of the flow, pressure,
heat and pump power.  These are useful, both to execute on-the-fly, manually
controlled shot profiles, as well as to carry out various maintenance operations
(rinsing the group headed, backflushing, etc.).  Note that no manual control
over the scale is necessary, as it tares automatically, whenever a large enough
change in mass is detected (e.g. a cup or bean dosing tray is placed on it).

## USB console

With the controller connected to a computer via USB, a rudimentary serial
console is available, affording some manual control of the executing firmware,
as well as querying and logging its state.  How it can be accessed, depends on
your platform.  For instance, on GNU/Linux, it can be accomplished with the
`socat` program with:

```bash
$ socat - /dev/ttyACM0,raw,echo=0
```

The following commands are available:

* `r`: Reset the controller.
* `b`: Reboot the controller into programming mode.

* `l[t|p|m|f][N]`: Toggle logging of temperature (`t`),
    pressure (`p`), mass (`m`), or flow (`f`), or print `N` log lines if an
    optional trailing number is provided.  The output consists of lines of
    comma-separated values, corresponding to time, value, derivative, raw value,
    and raw code, respectively.  The value is the filtered instantaneous value
    of the logged parameter, with the unfiltered value also available, to aid in
    tuning the filtering if desired.  The raw code is the data read from the
    sensor; mostly useful in debugging sensor communication.  Free-running
    logging can be stopped by issuing any logging command, including a simple
    `l`.

    For example `lt10` would display 10 lines of temperature data, `lp` would
    turn on pump logging and `l` would turn it off again.

* `l(T|P|F)[N]`: Toggle logging of temperature (`T`), pressure
    (`P`), or flow (`F`) PID control.  Similar to the above, only the
    lines contain time, PID output (i.e. heat/pump power), PID input
    (i.e. temperature, pressure, etc.), PID set point and PID proportional,
    integral and derivative terms.  Useful in tuning the PID parameters.

* `ls[N]`: Toggle "shot" logging.  Similar to the above, but the lines contain
    time, temperature, pressure, flow, volume, mass and heat and pump power.
    Useful in producing shot graphs, such as the one in this document.

* `lc`: Toggles "click" logging, useful in debugging the operation of the
    rotary encoder wheel.

* `s[h|p][d|p|f][N]`: Without a setting `N`, prints the current heat (`h`) or
    pump (`p`), delay (`d`), power (`p`) or flow (`f`) setting.  All are
    expressed in percentages, from 0 to 100.  Delay is simply the dalay before
    the TRIAC turns on the device in question, i.e. the complement of duty.
    Power is a setting in terms of percent of full power, from which a delay
    setting is calculated.  Flow, is only accepted for the pump and is the same
    as power, with a slight normalization as anything below 20% power doesn't
    really produce any flow to speak ok.

    For example, `shp` would print the current heat power setting and `spd50`
    would turn on power to the pump for half the mains voltage cycle.

* `c[h|p|f][SET,Kp,Ti,Td]`: With no settings, prints current heat (`h`) pump
    (`p`), or flow (`f`) PID configuration as a series of comma-separated
    numbers, including set point, gain, integration time, derivative time and
    current error integral of the PID.  The first three of the above parameters
    can be changed, by specifying new values.  The integral is always reset when
    any PID configuration value is changed.

    For example, `ch94` will only change the temperature setpoint to 94 keeping
    other heat PID parameters unchanged, while `cp3,0.35` will set the pump PID
    to 3 bars with gain equal to 0.35.

* `ci[SLAVE,ADDR,VAL]`: Query or configure I2C devices.  With only the slave
    specified, probe this slave.  This will result in an error if the slave is
    not on-line.  If a register address is also specified, read this address and
    return its contents in decimal, hexadecimal and binary forms.  If a value is
    also specified, write this value to the specified address. All parameters
    are in hexadecimal notation.

    For example, `ci54,0` reads register 0 of the NAU78802, while `cida,1,20`
    writes 0x20 to register 1 of the NSA2862X inside the pressure sensor.

* `z[f|m]`: Resets the calculated volume (`f`) to zero, or tares mass
    (`m`).

* `p[,STAGE,...]`: Without any arguments, a plain `p` prints the current
    programmed brew profile, otherwise programs the new brew profile.

    The program, consists of a set of stages, separated by commas.  Each stage
    is a separate phase of the brew process, where some output variable, which
    can be either flow (`f`), pressure (`p`) or pump power (`w`) is controlled
    based on some measured input variable, either shot time (`t`), current
    pressure (`p`), current volume (`v`), or current mass (`m`).  Conecptually,
    it can be though of as a graph, with the input (measured) variable on the x
    axis and the output (controlled) variable on the y axis.  Both axes can be
    specified in one of three modes: absolute (`a`) where the value is given in
    absolute terms, relative (`r`) where the value is given ralative to what it
    was at the beginning of the phase and ratiometric (`q`) where it is
    specified as a multiple of what it was at the beginninng of the phase.

    Each phase is specified as `input;output;point1;point2;...;commands`, where
    `input` and `output` each consist of two letters, specifying the input and
    ouput mode and variable, points are specified as `(x,y)` and commands can be
    one or more letters indicating that the shot time (`t`), volume (`v`), or
    mass (`m`) should be reset at the end of the phase.  The output value of the
    first point on each phase may be absent, indicating that the current value
    should be used.

    Consider the following program for example:

    ```
    ap;af;(0,3);(4,3);v,rt;ap;(0,);(1,0);(30,0);(35,9);(40,9);,av;qf;(0,);(+inf,1);
    ```

    It has three phases: the first preinfusion phase (`ap;af;(0,3);(4,3);v`)
    controls flow based on pressure and specifies a constant flow of 3 ml/s
    until the pressure reaches 4 bars.  The volume is then reset as we don't
    care about the water in the puck, only the yield.  The next phase
    (`rt;ap;(0,);(1,0);(30,0);(35,9);(40,9);`), controls pressure base on
    relative time, dropping the pressure to 0 within 1 second and keeping it
    there for 29 more seconds, until half a minute has passed from the beginning
    of the second phase.  The pressure is then ramped up to 9 bars in 5s and
    kept there for 5s more.  Finally the third phase (`rt;qf;(0,);(+inf,1);`)
    controls ratiometric flow based on relative time, and essentially keeps
    whatever flow resulted from the ramp-up to 9 bars at the end of the previous
    phase constant (a ratio of 1) forever, which is to say, until the brew
    switch is turned to the off position.

# Things

|STL|Description|
|---|-----------|
|[things/enclosure.stl](./things/enclosure.stl)|The enclosure for the controller board and peripherals.  It is mounted onto the side of the espresso machine.|
|[things/controller_bracket.stl](./things/controller_bracket.stl)|A bracket that can be mounted on the controller board, to serve as a tying post for the wiring.|
|[things/psu_bracket.stl](./things/psu_bracket.stl)|Same as above, for the PSU board.|
|[things/mount_gasket.stl](./things/mount_gasket.stl)|A gasket, to be printed in TPU and placed between the enclosure and the espresso machine's chassis.|
|[things/mount_washer.stl](./things/mount_washer.stl)|At least three of these are necessary; four if not using the pressure sensor mount.  They're used as washers for the M3 countersunk screws fastening the enclosure onto the machine's side.|
|[things/knob.stl](./things/knob.stl)|A knob for the rotary enocoder.  This is optional; any compatible knob can be used.|
|[things/knob_sleeve.stl](./things/knob_sleeve.stl)|A rubber sleeve for the knob, to be printed in TPU.|
|[things/nut.stl](./things/nut.stl)|An M7 flanged nut for the rotary encoder.  Not needed if one was supplied with the encoder.|
|[things/usb_plug.stl](./things/usb_plug.stl)|A plug for the USB port opening, to be printed in TPU.|
|[things/sensor_mount.stl](./things/sensor_mount.stl)|A mount bracket for the pressure sensor.|
|[things/load_base.stl](./things/load_base.stl)|The base of the weighing assembly, mounted in the place of the OEM drip tray.|
|[things/load_pin.stl](./things/load_pin.stl)|Locating pins, fastened with M4 countersunk screws on the bottom of the base, mating with the holes on the bottom of the espresso machine.|
|[things/load_plate.stl](./things/load_plate.stl)|The top plate, mounted on the load cell.|
|[things/load_plate_frame.stl](./things/load_plate_frame.stl)|A rubber frame for the top plate, to be printed in TPU and glued onto it.|

These designs were created using [Gamma](https://github.com/dpapavas/gamma) and
can therefore be modified to suit different parts or layouts.  Their source
files are located in
[/scheme](https://github.com/dpapavas/brew-controller/tree/master/scheme).

# Simulations

The simulations in [spice/](./spice) can be run with
[ngspice](https://ngspice.sourceforge.io/) and
[lepton-eda](https://github.com/lepton-eda/lepton-eda).  They include
simulations for the TRIAC drivers and RC snubber and can be useful when trying
out modifications to these circuits.

The following files are also needed to run the simulations, but haven't been
added to the distribution, since they are proprietary.  They can be readily
downloaded from the part manufacturers websites:

- `spice/1n4007.rev0.lib`
- `spice/2n7000.rev0.lib`
- `spice/MOC302X.lib`
- `spice/st_standard_snubberless_triacs.lib`

## License

The Scheme code in [scheme/](./scheme), is distributed under the [GNU
Affero General Public License Version 3](./LICENSE.AGPL).

The firmware sources in [src/](./src), as well as the controller PCB
design residing in [kicad/](./kicad/), is distributed under the [GNU
General Public License Version 3](./LICENSE.GPL).
