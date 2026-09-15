// Independently replay every grid point through the normal C simulation, then
// verify the optimizer's sample, four selections and complete Pareto ranking.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import vm from 'node:vm';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const executable = process.env.TXSIM_EXECUTABLE || path.join(root, 'build', process.platform === 'win32' ? 'txsim.exe' : 'txsim');
const run = args => {
  const result = spawnSync(executable, [...args, '--stdout-json', '--no-files'], { cwd: root, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr || result.error?.message);
  return JSON.parse(result.stdout);
};
const config = Object.fromEntries(fs.readFileSync(path.join(root, 'data/config.txt'), 'utf8').split(/\r?\n/)
  .filter(line => /^[A-Z_0-9]+\s*=/.test(line)).map(line => line.split('=').map(s => s.trim())));
const bound = (key, fallback) => Number(config[key] ?? fallback);
const ranges = [
  ['EMF_VALUE_FACTOR', bound('OPTIMIZER_K_MIN', 0.6), bound('OPTIMIZER_K_MAX', 0.65), bound('OPTIMIZER_K_STEPS', 6)],
  ['CORE_FLUX_DENSITY', bound('OPTIMIZER_BM_MIN', 1.4), bound('OPTIMIZER_BM_MAX', 1.7), bound('OPTIMIZER_BM_STEPS', 7)],
  ['AVERAGE_CURRENT_DENSITY', bound('OPTIMIZER_CURRENT_DENSITY_MIN', 2.3), bound('OPTIMIZER_CURRENT_DENSITY_MAX', 3.2), bound('OPTIMIZER_CURRENT_DENSITY_STEPS', 7)],
  ['WINDOW_ASPECT_RATIO', bound('OPTIMIZER_ASPECT_RATIO_MIN', 2.5), bound('OPTIMIZER_ASPECT_RATIO_MAX', 4), bound('OPTIMIZER_ASPECT_RATIO_STEPS', 6)]
];
const output = run(['--mode', 'optimize']);
const o = output.optimization;
const total = ranges.reduce((n, r) => n * r[3], 1);
assert.equal(o.evaluatedDesigns, total);
assert.equal(o.samples.length, 15);
assert.deepEqual(o.samples.map(c => c.serialNumber), Array.from({ length: 15 }, (_, i) => 1 + Math.round(i * (total - 1) / 14)));
const close = (actual, expected) => assert.ok(Math.abs(actual - expected) <= 2e-6 * Math.max(1, Math.abs(expected)), `${actual} != ${expected}`);
const all = [];
for (let k = 0; k < ranges[0][3]; k++) for (let b = 0; b < ranges[1][3]; b++) for (let j = 0; j < ranges[2][3]; j++) for (let a = 0; a < ranges[3][3]; a++) {
  const args = ['--mode', 'explore', '--set', 'AUTOMATIC_CONDUCTOR_SIZING=1'];
  [k, b, j, a].forEach((index, dimension) => {
    const [key, min, max, count] = ranges[dimension];
    args.push('--set', `${key}=${count === 1 ? min : min + (max - min) * index / (count - 1)}`);
  });
  const tx = run(args), { frame: f, lv, hv, tank: t, noLoad: n, performance: p } = tx.sections;
  const serialNumber = all.length + 1;
  const limits = o.constraints;
  const feasible = lv.currentDensity >= limits.actualCurrentDensityMin && lv.currentDensity <= limits.actualCurrentDensityMax && hv.currentDensity >= limits.actualCurrentDensityMin && hv.currentDensity <= limits.actualCurrentDensityMax &&
    lv.axialSlackMm >= limits.minimumAxialSlackMm && hv.axialSlackMm >= limits.minimumAxialSlackMm && hv.endCoilTurns > 0 && f.centreDistanceM * 1000 - hv.outerDiameterMm >= limits.minimumAdjacentClearanceMm &&
    t.cooledRiseC <= t.targetRiseC + limits.temperatureMarginC && t.oilVolumeM3 > 0 && p.cases[1].efficiencyPercent >= limits.minimumEfficiencyPercent &&
    t.specificMassKgKva <= limits.maximumSpecificMassKgKva && n.noLoadCurrentPercent <= limits.maximumNoLoadCurrentPercent && t.volumeM3 <= limits.maximumTankVolumeM3;
  const c = { serialNumber, feasible, comparisonEfficiencyPercent: p.cases[1].efficiencyPercent,
    specificMassKgKva: t.specificMassKgKva, noLoadCurrentPercent: n.noLoadCurrentPercent, tankVolumeM3: t.volumeM3,
    totalLossW: tx.summary.totalLossW, activeMassKg: t.activeMassKg, materialCostIndex: t.materialCostIndex };
  all.push(c);
  const sample = o.samples.find(item => item.serialNumber === serialNumber);
  if (sample) {
    assert.equal(sample.feasible, feasible, `Feasibility at Sn ${serialNumber}`);
    for (const key of ['comparisonEfficiencyPercent', 'specificMassKgKva', 'noLoadCurrentPercent', 'tankVolumeM3']) close(sample[key], c[key]);
  }
}
const feasible = all.filter(c => c.feasible);
assert.equal(o.feasibleDesigns, feasible.length);
for (const [key, field, sign] of [['efficiency', 'comparisonEfficiencyPercent', -1], ['specificMass', 'specificMassKgKva', 1], ['noLoadCurrent', 'noLoadCurrentPercent', 1], ['tankVolume', 'tankVolumeM3', 1]]) {
  const best = [...feasible].sort((a, b) => sign * (a[field] - b[field]) || a.serialNumber - b.serialNumber)[0];
  assert.equal(o.criteriaWinners[key]?.serialNumber, best?.serialNumber);
  const diagnostic = [...all].sort((a, b) => sign * (a[field] - b[field]) || a.serialNumber - b.serialNumber)[0];
  assert.equal(o.calculatedCriteriaWinners[key]?.serialNumber, diagnostic?.serialNumber);
}
const objectives = ['totalLossW', 'activeMassKg', 'materialCostIndex'];
const frontier = feasible.filter(c => !feasible.some(d => objectives.every(k => d[k] <= c[k]) && objectives.some(k => d[k] < c[k])));
assert.equal(o.paretoCount, frontier.length);
for (const c of frontier) c.score = Math.sqrt(objectives.reduce((sum, key) => {
  const min = Math.min(...frontier.map(v => v[key])), max = Math.max(...frontier.map(v => v[key]));
  return sum + (max > min ? (c[key] - min) / (max - min) : 0) ** 2;
}, 0));
frontier.sort((a, b) => a.score - b.score || a.serialNumber - b.serialNumber);
assert.deepEqual(o.candidates.map(c => c.serialNumber), frontier.slice(0, 32).map(c => c.serialNumber));
const empty = run(['--mode', 'optimize', '--set', 'OPTIMIZER_CURRENT_DENSITY_MIN=20', '--set', 'OPTIMIZER_CURRENT_DENSITY_MAX=21']).optimization;
assert.equal(empty.feasibleDesigns, 0);
assert.equal(empty.recommendedIndex, -1);
assert.equal(empty.samples.length, 15);
assert.deepEqual(Object.values(empty.criteriaWinners), [null, null, null, null]);
assert.deepEqual(empty.candidates, []);
const textbook = run(['--mode', 'optimize', '--set', 'OPTIMIZER_PROFILE=textbook']).optimization;
assert.equal(textbook.profile, 'textbook');
assert.equal(textbook.recommendedIndex, -1);
assert.deepEqual(textbook.candidates, []);
assert.equal(textbook.paretoCount, 0);
assert.ok(textbook.feasibleDesigns > 0, 'Textbook profile should retain lecturer-style feasible rows for the default search');
const context = { window: {} };
vm.runInNewContext(fs.readFileSync(path.join(root, 'dist/optimization.js'), 'utf8'), context);
const ui = context.window.TX_OPTIMIZATION;
assert.equal(ui.section(o).slides.length, 8);
assert.equal((ui.render(o).match(/<tr/g) || []).length, 32);
assert.match(ui.render(empty), /CALCULATED/);
assert.match(ui.render(empty), /No balanced recommendation/);
assert.match(ui.render({}), /Run Optimal again/);
for (const result of [o, empty, textbook]) for (const slide of ui.section(result).slides) {
  assert.ok(ui.slide(slide, result).includes('<h2>'));
  assert.doesNotMatch(ui.slide(slide, result), /undefined|NaN/);
}
for (const candidate of [...Object.values(o.criteriaWinners), o.candidates[0]].filter(Boolean)) {
  const values = ui.replayOverrides(output, candidate.serialNumber);
  const replay = run(['--mode', 'explore', ...Object.entries(values).flatMap(([key, value]) => ['--set', `${key}=${value}`])]);
  close(replay.sections.performance.cases[1].efficiencyPercent, candidate.comparisonEfficiencyPercent);
  close(replay.sections.tank.specificMassKgKva, candidate.specificMassKgKva);
  close(replay.sections.noLoad.noLoadCurrentPercent, candidate.noLoadCurrentPercent);
  close(replay.sections.tank.volumeM3, candidate.tankVolumeM3);
  assert.equal(replay.assumptions.emfValueFactor, candidate.k);
}
const small = run(['--mode', 'optimize', '--set', 'OPTIMIZER_K_STEPS=1', '--set', 'OPTIMIZER_BM_STEPS=1', '--set', 'OPTIMIZER_CURRENT_DENSITY_STEPS=1', '--set', 'OPTIMIZER_ASPECT_RATIO_STEPS=1']).optimization;
assert.equal(small.evaluatedDesigns, 1);
assert.equal(small.samples.length, 1);
assert.equal(small.samples[0].serialNumber, 1);
console.log(`Verified all ${total} attempts, 15 samples, four global winners, ${frontier.length} Pareto variants, exact winner replay, single-point and empty-feasible searches.`);
