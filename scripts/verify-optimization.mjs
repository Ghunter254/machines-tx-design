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
  ['CORE_FLUX_DENSITY', bound('OPTIMIZER_BM_MIN', 1.4), bound('OPTIMIZER_BM_MAX', 1.7), 7],
  ['AVERAGE_CURRENT_DENSITY', bound('OPTIMIZER_CURRENT_DENSITY_MIN', 2.3), bound('OPTIMIZER_CURRENT_DENSITY_MAX', 3.2), 7],
  ['WINDOW_ASPECT_RATIO', bound('OPTIMIZER_ASPECT_RATIO_MIN', 2.5), bound('OPTIMIZER_ASPECT_RATIO_MAX', 4), 6]
];
const output = run(['--mode', 'optimize']);
const o = output.optimization;
assert.equal(o.evaluatedDesigns, 294);
assert.equal(o.samples.length, 15);
assert.deepEqual(o.samples.map(c => c.serialNumber), Array.from({ length: 15 }, (_, i) => 1 + Math.round(i * 293 / 14)));
const close = (actual, expected) => assert.ok(Math.abs(actual - expected) <= 2e-6 * Math.max(1, Math.abs(expected)), `${actual} != ${expected}`);
const all = [];
for (let b = 0; b < 7; b++) for (let j = 0; j < 7; j++) for (let a = 0; a < 6; a++) {
  const args = ['--mode', 'explore', '--set', 'AUTOMATIC_CONDUCTOR_SIZING=1'];
  [b, j, a].forEach((index, dimension) => {
    const [key, min, max, count] = ranges[dimension];
    args.push('--set', `${key}=${min + (max - min) * index / (count - 1)}`);
  });
  const tx = run(args), { frame: f, lv, hv, tank: t, noLoad: n, performance: p } = tx.sections;
  const serialNumber = all.length + 1;
  const feasible = lv.currentDensity >= 2.3 && lv.currentDensity <= 3.5 && hv.currentDensity >= 2.3 && hv.currentDensity <= 3.5 &&
    lv.axialSlackMm >= 7 && hv.axialSlackMm >= 7 && hv.endCoilTurns > 0 && f.centreDistanceM * 1000 - hv.outerDiameterMm >= 15 &&
    t.cooledRiseC <= t.targetRiseC + 0.05 && t.oilVolumeM3 > 0;
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
const context = { window: {} };
vm.runInNewContext(fs.readFileSync(path.join(root, 'dist/optimization.js'), 'utf8'), context);
const ui = context.window.TX_OPTIMIZATION;
assert.equal(ui.section(o).slides.length, 8);
assert.equal((ui.render(o).match(/<tr/g) || []).length, 16);
assert.match(ui.render(empty), /No feasible variant/);
assert.match(ui.render({}), /Run Optimal again/);
for (const result of [o, empty]) for (const slide of ui.section(result).slides) {
  assert.ok(ui.slide(slide, result).includes('<h2>'));
  assert.doesNotMatch(ui.slide(slide, result), /undefined|NaN/);
}
console.log(`Verified all 294 attempts, 15 samples, four global winners, ${frontier.length} Pareto variants, and the empty-feasible search.`);
