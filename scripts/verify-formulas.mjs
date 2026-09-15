import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const data = JSON.parse(fs.readFileSync(process.argv[2] || path.join(root, 'data', 'output.json'), 'utf8'));
const { input: i, assumptions: a, sections: s } = data;
const f = s.frame;
const n = s.noLoad;
const lv = s.lv;
const hv = s.hv;
const p = s.performance;
const t = s.tank;
const checks = [];

const close = (label, actual, expected, tolerance = 3e-6) => {
  const scale = Math.max(1, Math.abs(expected));
  const error = Math.abs(actual - expected) / scale;
  checks.push({ label, actual, expected, error });
  if (!Number.isFinite(actual) || error > tolerance) {
    throw new Error(`${label}: received ${actual}, expected ${expected} (relative error ${error})`);
  }
};

const roundUp = (value, step) => step > 0 ? Math.ceil((value - 1e-12) / step) * step : value;
const phaseVoltage = (lineVoltage, connection) => String(connection).toLowerCase() === 'delta' ? lineVoltage : lineVoltage / Math.sqrt(3);
const phaseCurrent = (kva, lineVoltage, phases, connection) => kva * 1000 / (phases * phaseVoltage(lineVoltage, connection));

// Magnetic frame - textbook 5.2.2 with configurable assumptions.
close('frame.netCoreAreaM2', f.netCoreAreaM2, a.coreStepFactor * f.coreDiameterM ** 2);
close('frame.voltsPerTurn', f.voltsPerTurn, 4.44 * i.frequency * i.coreFluxDensityT * f.netCoreAreaM2);
close('frame.grossCoreAreaM2', f.grossCoreAreaM2, f.netCoreAreaM2 / a.stackingFactor);
close('frame.windowSpaceFactor', f.windowSpaceFactor, 10 / (30 + i.hvVoltage / 1000) * a.windowSpaceMultiplier);
close('frame.windowAreaM2', f.windowAreaM2, i.kva * 1000 / (3.33 * i.frequency * i.coreFluxDensityT * f.windowSpaceFactor * i.averageCurrentDensity * 1e6 * f.netCoreAreaM2));
close('frame.coreLengthM', f.coreLengthM, roundUp(Math.sqrt(i.windowAspectRatio * f.windowAreaM2), a.dimensionRoundingM));
close('frame.centreDistanceM', f.centreDistanceM, roundUp(f.windowAreaM2 / f.coreLengthM + f.coreDiameterM, a.dimensionRoundingM));
close('frame.windowRatio', f.windowRatio, f.coreLengthM / (f.centreDistanceM - f.coreDiameterM));
close('frame.yokeLengthM', f.yokeLengthM, roundUp(2 * f.centreDistanceM + a.yokeWidthFactor * f.coreDiameterM, a.dimensionRoundingM));
close('frame.yokeGrossAreaM2', f.yokeGrossAreaM2, a.yokeAreaFactor * f.grossCoreAreaM2);
close('frame.yokeWidthM', f.yokeWidthM, a.yokeWidthFactor * f.coreDiameterM);
close('frame.yokeHeightM', f.yokeHeightM, f.yokeGrossAreaM2 / f.yokeWidthM);
close('frame.yokeFluxDensityT', f.yokeFluxDensityT, f.grossCoreAreaM2 / f.yokeGrossAreaM2 * i.coreFluxDensityT);
close('frame.coreLossWPerKg', f.coreLossWPerKg, a.coreLossReferenceWKg * (i.coreFluxDensityT / a.coreLossReferenceFluxT) ** a.lossCurveExponent);
close('frame.yokeLossWPerKg', f.yokeLossWPerKg, a.yokeLossReferenceWKg * (f.yokeFluxDensityT / a.yokeLossReferenceFluxT) ** a.lossCurveExponent);
close('frame.coreMassKg', f.coreMassKg, i.phases * f.grossCoreAreaM2 * f.coreLengthM * a.steelDensityKgM3);
close('frame.yokeMassKg', f.yokeMassKg, 2 * f.yokeGrossAreaM2 * f.yokeLengthM * a.steelDensityKgM3);
close('frame.coreIronLossW', f.coreIronLossW, f.coreLossWPerKg * f.coreMassKg);
close('frame.yokeIronLossW', f.yokeIronLossW, f.yokeLossWPerKg * f.yokeMassKg);
close('frame.ironLossW', f.ironLossW, a.ironLossBuildFactor * (f.coreIronLossW + f.yokeIronLossW));
close('frame.totalIronMassKg', f.totalIronMassKg, f.coreMassKg + f.yokeMassKg);
close('frame.totalIronVolumeM3', f.totalIronVolumeM3, f.totalIronMassKg / a.steelDensityKgM3);

// No-load current - textbook 5.2.3, generalized by winding connection.
const lvPhaseVoltage = phaseVoltage(i.lvVoltage, i.lvConnection);
close('noLoad.coreAmpereTurns', n.coreAmpereTurns, i.phases * n.coreATPerM * f.coreLengthM);
close('noLoad.yokeAmpereTurns', n.yokeAmpereTurns, 2 * n.yokeATPerM * f.yokeLengthM);
close('noLoad.totalATPerPhase', n.totalATPerPhase, (n.coreAmpereTurns + n.yokeAmpereTurns) / i.phases);
close('noLoad.lvPhaseVoltageV', n.lvPhaseVoltageV, lvPhaseVoltage);
close('noLoad.lvTurns', n.lvTurns, Math.ceil(lvPhaseVoltage / f.voltsPerTurn));
close('noLoad.lvPhaseCurrentA', n.lvPhaseCurrentA, phaseCurrent(i.kva, i.lvVoltage, i.phases, i.lvConnection));
close('noLoad.wattfulCurrentA', n.wattfulCurrentA, f.ironLossW / (i.phases * lvPhaseVoltage));
close('noLoad.magnetizingCurrentA', n.magnetizingCurrentA, a.excitationBuildFactor * n.totalATPerPhase / (Math.sqrt(2) * n.lvTurns));
close('noLoad.noLoadCurrentA', n.noLoadCurrentA, Math.hypot(n.wattfulCurrentA, n.magnetizingCurrentA));
close('noLoad.noLoadCurrentPercent', n.noLoadCurrentPercent, n.noLoadCurrentA / n.lvPhaseCurrentA * 100);

// LV winding - textbook 5.2.4 plus reference-temperature correction.
close('lv.phaseVoltageV', lv.phaseVoltageV, lvPhaseVoltage);
close('lv.turns', lv.turns, n.lvTurns);
close('lv.phaseCurrentA', lv.phaseCurrentA, n.lvPhaseCurrentA);
close('lv.availableWindingHeightMm', lv.availableWindingHeightMm, a.lvWindingHeightFraction * f.coreLengthM * 1000);
close('lv.turnsAxially', lv.turnsAxially, Math.ceil(lv.turns / lv.turnsRadially));
close('lv.radialStrands', lv.radialStrands, lv.parallelStrands / lv.axialStrands);
close('lv.spacePerTurnMm', lv.spacePerTurnMm, lv.availableWindingHeightMm / lv.turnsAxially);
close('lv.nominalConductorAreaMm2', lv.nominalConductorAreaMm2, lv.strandWidthMm * lv.strandThicknessMm * lv.parallelStrands);
close('lv.conductorAreaMm2', lv.conductorAreaMm2, lv.nominalConductorAreaMm2 * a.lvEdgeFactor);
close('lv.currentDensity', lv.currentDensity, lv.phaseCurrentA / lv.conductorAreaMm2);
close('lv.activeHeightMm', lv.activeHeightMm, ((lv.strandWidthMm + a.conductorInsulationMm) * lv.axialStrands + a.lvInterTurnInsulationMm) * lv.turnsAxially);
close('lv.occupiedHeightMm', lv.occupiedHeightMm, lv.activeHeightMm + a.lvEndInsulationMm);
close('lv.axialSlackMm', lv.axialSlackMm, f.coreLengthM * 1000 - lv.occupiedHeightMm);
close('lv.radialWidthMm', lv.radialWidthMm, lv.radialStrands * (lv.strandThicknessMm + a.conductorInsulationMm) * lv.turnsRadially + a.lvRadialInsulationMm);
close('lv.innerDiameterMm', lv.innerDiameterMm, f.coreDiameterM * 1000 + 2 * (a.coreToLvOilDuctMm + a.coreToLvCylinderMm + a.lvFormerToWindingDuctMm));
close('lv.outerDiameterMm', lv.outerDiameterMm, lv.innerDiameterMm + 2 * lv.radialWidthMm);
close('lv.meanTurnLengthM', lv.meanTurnLengthM, Math.PI * (lv.innerDiameterMm + lv.outerDiameterMm) / 2000);
close('lv.conductorLengthM', lv.conductorLengthM, lv.meanTurnLengthM * lv.turns);
close('lv.copperVolumeM3', lv.copperVolumeM3, lv.conductorLengthM * lv.conductorAreaMm2 * 1e-6);
close('lv.copperMassKg', lv.copperMassKg, lv.copperVolumeM3 * a.copperDensityKgM3);
close('lv.resistance20Ohm', lv.resistance20Ohm, a.copperResistivity20 * lv.conductorLengthM / lv.conductorAreaMm2);
close('lv.resistanceReferenceOhm', lv.resistanceReferenceOhm, lv.resistance20Ohm * (a.copperTemperatureConstant + i.referenceTemperatureC) / (a.copperTemperatureConstant + 20));
close('lv.copperLoss20Kw', lv.copperLoss20Kw, i.phases * lv.phaseCurrentA ** 2 * lv.resistance20Ohm / 1000);
close('lv.copperLossReferenceKw', lv.copperLossReferenceKw, i.phases * lv.phaseCurrentA ** 2 * lv.resistanceReferenceOhm / 1000);

// HV winding - textbook 5.2.5, generalized by winding connection.
const hvPhaseVoltage = phaseVoltage(i.hvVoltage, i.hvConnection);
close('hv.phaseVoltageV', hv.phaseVoltageV, hvPhaseVoltage);
close('hv.turns', hv.turns, Math.ceil(lv.turns * hvPhaseVoltage / lvPhaseVoltage));
close('hv.phaseCurrentA', hv.phaseCurrentA, phaseCurrent(i.kva, i.hvVoltage, i.phases, i.hvConnection));
close('hv.availableWindingHeightMm', hv.availableWindingHeightMm, a.hvWindingHeightFraction * f.coreLengthM * 1000);
close('hv.middleCoilTurns', hv.middleCoilTurns, hv.axialStrands * hv.radialStrands);
close('hv.endCoilTurns', hv.endCoilTurns, (hv.turns - hv.middleCoilTurns * (hv.coils - 2)) / 2);
close('hv.spacePerCoilMm', hv.spacePerCoilMm, hv.availableWindingHeightMm / hv.coils);
close('hv.nominalConductorAreaMm2', hv.nominalConductorAreaMm2, hv.strandWidthMm * hv.strandThicknessMm);
close('hv.conductorAreaMm2', hv.conductorAreaMm2, hv.nominalConductorAreaMm2 * a.hvEdgeFactor);
close('hv.currentDensity', hv.currentDensity, hv.phaseCurrentA / hv.conductorAreaMm2);
close('hv.activeHeightMm', hv.activeHeightMm, hv.coils * hv.axialStrands * (hv.strandWidthMm + a.conductorInsulationMm) + (hv.coils - 1) * a.hvInterCoilInsulationMm);
close('hv.occupiedHeightMm', hv.occupiedHeightMm, hv.activeHeightMm + a.hvEndRingMm + a.hvEndInsulationMm);
close('hv.axialSlackMm', hv.axialSlackMm, f.coreLengthM * 1000 - hv.occupiedHeightMm);
close('hv.radialWidthMm', hv.radialWidthMm, hv.radialStrands * (hv.strandThicknessMm + a.conductorInsulationMm));
close('hv.innerDiameterMm', hv.innerDiameterMm, lv.outerDiameterMm + 2 * (a.lvToHvOilDuctMm + a.lvToHvCylinderMm + a.hvFormerToWindingDuctMm));
close('hv.outerDiameterMm', hv.outerDiameterMm, hv.innerDiameterMm + 2 * hv.radialWidthMm);
close('hv.adjacentClearanceMm', hv.adjacentClearanceMm, f.centreDistanceM * 1000 - hv.outerDiameterMm);
close('hv.meanTurnLengthM', hv.meanTurnLengthM, Math.PI * (hv.innerDiameterMm + hv.outerDiameterMm) / 2000);
close('hv.conductorLengthM', hv.conductorLengthM, hv.meanTurnLengthM * hv.turns);
close('hv.copperVolumeM3', hv.copperVolumeM3, hv.conductorLengthM * hv.conductorAreaMm2 * 1e-6);
close('hv.copperMassKg', hv.copperMassKg, hv.copperVolumeM3 * a.copperDensityKgM3);
close('hv.resistance20Ohm', hv.resistance20Ohm, a.copperResistivity20 * hv.conductorLengthM / hv.conductorAreaMm2);
close('hv.resistanceReferenceOhm', hv.resistanceReferenceOhm, hv.resistance20Ohm * (a.copperTemperatureConstant + i.referenceTemperatureC) / (a.copperTemperatureConstant + 20));
close('hv.copperLoss20Kw', hv.copperLoss20Kw, i.phases * hv.phaseCurrentA ** 2 * hv.resistance20Ohm / 1000);
close('hv.copperLossReferenceKw', hv.copperLossReferenceKw, i.phases * hv.phaseCurrentA ** 2 * hv.resistanceReferenceOhm / 1000);

// Performance - textbook 5.2.6.
close('performance.copperLoss20Kw', p.copperLoss20Kw, a.copperStrayLossFactor * (lv.copperLoss20Kw + hv.copperLoss20Kw));
close('performance.copperLossReferenceKw', p.copperLossReferenceKw, a.copperStrayLossFactor * (lv.copperLossReferenceKw + hv.copperLossReferenceKw));
close('performance.totalLossKw', p.totalLossKw, p.ironLossKw + p.copperLossReferenceKw);
p.cases.forEach((item, index) => {
  close(`performance.cases[${index}].lossKw`, item.lossKw, p.ironLossKw + p.copperLossReferenceKw * item.loadPu ** 2);
  close(`performance.cases[${index}].outputKw`, item.outputKw, item.loadPu * i.kva * item.powerFactor);
  close(`performance.cases[${index}].inputKw`, item.inputKw, item.outputKw + item.lossKw);
  close(`performance.cases[${index}].efficiencyPercent`, item.efficiencyPercent, item.outputKw / item.inputKw * 100);
});
close('performance.maxEfficiencyLoadKva', p.maxEfficiencyLoadKva, Math.sqrt(p.ironLossKw / p.copperLossReferenceKw) * i.kva);
close('performance.maxEfficiencyPercent', p.maxEfficiencyPercent, p.maxEfficiencyLoadKva * 0.85 / (p.maxEfficiencyLoadKva * 0.85 + 2 * p.ironLossKw) * 100);
close('performance.meanTurnLengthM', p.meanTurnLengthM, (lv.meanTurnLengthM + hv.meanTurnLengthM) / 2);
close('performance.coilLengthM', p.coilLengthM, hv.activeHeightMm / 1000);
close('performance.ampereTurnsPerPhase', p.ampereTurnsPerPhase, hv.phaseCurrentA * hv.turns);
const expectedEx = 2 * Math.PI * i.frequency * 4 * Math.PI * 1e-7 * p.meanTurnLengthM * p.ampereTurnsPerPhase / (p.coilLengthM * f.voltsPerTurn) * (0.016 + (hv.radialWidthMm + lv.radialWidthMm) / 3000);
close('performance.reactancePu', p.reactancePu, expectedEx);
close('performance.resistancePu', p.resistancePu, p.copperLossReferenceKw / i.kva);
close('performance.impedancePu', p.impedancePu, Math.hypot(p.resistancePu, p.reactancePu));
close('performance.regulation85Pu', p.regulation85Pu, p.resistancePu * 0.85 + p.reactancePu * Math.sqrt(1 - 0.85 ** 2));
close('performance.regulationUnityPu', p.regulationUnityPu, p.resistancePu);

// Tank and cooling - textbook 5.2.7 plus labelled project extensions.
close('tank.lengthM', t.lengthM, 2 * f.centreDistanceM + hv.outerDiameterMm / 1000 + t.lengthClearanceM);
close('tank.widthM', t.widthM, hv.outerDiameterMm / 1000 + t.widthClearanceM);
close('tank.heightM', t.heightM, f.coreLengthM + 2 * f.yokeHeightM + t.heightClearanceM);
close('tank.volumeM3', t.volumeM3, t.lengthM * t.widthM * t.heightM);
close('tank.surfaceAreaM2', t.surfaceAreaM2, 2 * (t.widthM + t.lengthM) * t.heightM);
close('tank.plainRiseC', t.plainRiseC, p.totalLossKw * 1000 / (a.plainTankDissipationWm2C * t.surfaceAreaM2));
close('tank.tubeAreaM2', t.tubeAreaM2, Math.PI * t.tubeDiameterM * t.tubeHeightM);
const remainingHeat = p.totalLossKw * 1000 - a.plainTankDissipationWm2C * t.surfaceAreaM2 * t.targetRiseC;
const expectedTubeArea = Math.max(0, remainingHeat / (a.tubeCoefficientWm2C * t.targetRiseC * a.tubeEffectiveness));
close('tank.requiredTubeAreaM2', t.requiredTubeAreaM2, expectedTubeArea);
close('tank.coolingTubes', t.coolingTubes, expectedTubeArea > 0 ? Math.ceil(expectedTubeArea / t.tubeAreaM2) : 0);
const totalDissipation = a.plainTankDissipationWm2C * t.surfaceAreaM2 + a.tubeCoefficientWm2C * a.tubeEffectiveness * t.coolingTubes * t.tubeAreaM2;
close('tank.cooledRiseC', t.cooledRiseC, p.totalLossKw * 1000 / totalDissipation);
close('tank.activeMassKg', t.activeMassKg, 1.01 * (t.hvCopperMassKg + t.lvCopperMassKg + t.ironMassKg));
close('tank.hvCopperMassKg', t.hvCopperMassKg, i.phases * hv.copperMassKg);
close('tank.lvCopperMassKg', t.lvCopperMassKg, i.phases * lv.copperMassKg);
close('tank.specificMassKgKva', t.specificMassKgKva, t.activeMassKg / i.kva);
const tankPlateArea = t.surfaceAreaM2 + 2 * t.lengthM * t.widthM;
close('tank.steelMassKg', t.steelMassKg, a.steelDensityKgM3 * tankPlateArea * a.tankPlateThicknessM);
close('tank.oilVolumeM3', t.oilVolumeM3, Math.max(0, t.volumeM3 - f.totalIronVolumeM3 - i.phases * (lv.copperVolumeM3 + hv.copperVolumeM3)));
close('tank.oilMassKg', t.oilMassKg, a.oilDensityKgM3 * t.oilVolumeM3);
close('tank.shippingMassKg', t.shippingMassKg, t.activeMassKg + t.steelMassKg + t.oilMassKg);
close('tank.materialCostIndex', t.materialCostIndex, (t.hvCopperMassKg + t.lvCopperMassKg) * a.copperCostIndex + t.ironMassKg * a.coreSteelCostIndex + t.steelMassKg * a.tankSteelCostIndex + t.oilMassKg * a.oilCostIndex);

console.log(`Verified ${checks.length} formula relationships across all six calculation sections.`);
