# Transformer formula audit

This note reconciles the C calculation chain with sections 5.2.2–5.2.7 of `tx.pdf`. It records where the program deliberately generalizes the worked example and where the earlier result table contained presentation errors.

Run the repeatable audit after changing a calculation:

```text
node scripts/verify-formulas.mjs
```

The current test checks 127 relationships across the magnetic frame, no-load current, LV winding, HV winding, performance, and tank/cooling sections.

## Calculation chain

### 1. Magnetic frame — textbook 5.2.2

- Net iron area: `Ai = K d²`
- Voltage per turn: `Et = 4.44 f Bm Ai`
- Gross core area: `Ac = Ai / Ki`
- Window space factor: `Kw = 10 / (30 + kVHV)`, followed by the configured design multiplier
- Window area: `Aw = Q / (3.33 f Bm Kw δ Ai)` using consistent SI units
- Window height: `L = √(window ratio × Aw)`
- Distance between limb centres: `D = Aw/L + d`
- Yoke length: `W = 2D + 0.9d`
- Yoke gross area: `Ay = 1.15Ac`; `by = 0.9d`; `hy = Ay/by`
- Iron mass is density × gross volume. Iron loss is mass × loss per kilogram, with the configured build factor applied once.

### 2. No-load current — textbook 5.2.3

- Core ampere-turns: `ATC = 3 atC L`
- Yoke ampere-turns: `ATY = 2 atY W`
- Ampere-turns per phase: `ATpPh = (ATC + ATY)/3`
- Wattful current: `Iw = Pi/(3Vphase)`
- Magnetizing current: `Im = excitation factor × ATpPh/(√2 T2)`
- No-load current: `I0 = √(Iw² + Im²)`

The textbook acceptance band is 0.5–1.0% of rated LV phase current. The engine now applies both the lower and upper limits.

### 3. LV winding — textbook 5.2.4

- Connection-aware phase voltage and phase current are used.
- Turns: `T2 = ceil(Vphase/Et)` so the core is not over-fluxed by rounding down.
- Copper area: total nominal strand area × the configured edge/packing factor.
- Current density: `J2 = I2/a2`
- Mean turn length: `Lmt2 = π(di2 + do2)/2`
- Conductor length: `Lcu2 = T2 Lmt2`
- Volume, mass, resistance, and `3I²R` loss follow from that conductor length.

### 4. HV winding — textbook 5.2.5

- HV turns preserve the phase-voltage ratio and are rounded upward.
- HV phase current follows the selected star or delta connection.
- Disc-coil turns, conductor area, current density, build, diameters, mean turn length, copper volume, mass, resistance, and copper loss follow the textbook sequence.
- The engine also calculates adjacent winding-envelope clearance, which is needed to identify a physically congested design.

### 5. Performance — textbook 5.2.6

- Reference copper loss includes the configured stray-load factor.
- Total full-load loss: `ptFL = Pi + pcuT`
- Maximum-efficiency loading: `Smaxη = S √(Pi/pcuT)`
- Efficiency is calculated at the requested load and power factor using output divided by output plus losses.
- Per-unit resistance: `Er = pcuT/S`
- Per-unit impedance: `Ez = √(Er² + Ex²)`
- Regulation at 0.85 power factor: `Er×0.85 + Ex√(1−0.85²)`

### 6. Tank and cooling — textbook 5.2.7

- Tank dimensions come from the active-part envelope plus configured clearances.
- Vertical cooling surface: `St = 2(bt + Lt)ht`
- Plain-tank rise: `Tr = ptFL×1000/(kdiss St)`
- Cooling tube area: `At = πDctHct`
- Tube count is rounded upward so installed area is never below required area.
- Oil volume, tank-steel mass, shipping mass, and material cost index are clearly labelled project extensions.

## Reconciled inconsistencies

- `2D + 0.9d` is the yoke length `W`, not the core/window height `L`. The earlier table paired this formula with the 609 mm core-length result.
- `L + 2hy` is the overall active-frame height. The earlier table left its result blank.
- The LV conductor length near 22.8 is in metres, not millimetres.
- `stW × stT × stP` is an area in mm². It is not a one-dimensional conductor size.
- The HV entry written as `811 mm` represents an `8 × 1 mm` strand and an 8 mm² nominal area.
- The textbook prose and its sample program use different extra iron-loss allowances (5% and 7%). The software therefore exposes one explicit `IRON_LOSS_BUILD_FACTOR` instead of hiding either value.
- The worked example uses a rounded copper resistivity near 0.02 Ω·mm²/m. The program uses the configured 20°C value and separately reports resistance and copper loss at 20°C and at the configured reference temperature. These values should not be mixed in one loss total.
- The worked example uses 7550 kg/m³ for iron, while the current project configuration uses 7850 kg/m³. Mass differences are therefore assumption differences, not algebra errors.

## Engineering interpretation

Passing the formula audit proves internal numerical consistency with the selected assumptions; it does not certify a production transformer. The dashboard separately reports academic design checks and the local utility benchmark. A production release still needs verified insulation coordination, dielectric clearances, short-circuit withstand, detailed thermal design, acoustic/mechanical review, manufacturability, and applicable routine/type tests.
