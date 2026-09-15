/* Presentation-only formatting. Every result and selection comes from the C engine. */
window.TX_OPTIMIZATION = (() => {
  const fmt = (value, digits = 3) => value != null && Number.isFinite(Number(value))
    ? Number(value).toLocaleString('en', { minimumFractionDigits: digits, maximumFractionDigits: digits }) : '—';
  const escape = value => String(value ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  const criteria = [
    { key: 'efficiency', title: 'Maximum efficiency', field: 'comparisonEfficiencyPercent', unit: '%', meaning: 'Select this variant for the highest full-load efficiency at 0.85 power factor within the feasible search. This is not the separate maximum-efficiency operating load.', short: 'Highest efficiency at full load · 0.85 PF' },
    { key: 'specificMass', title: 'Minimum kg/kVA', field: 'specificMassKgKva', unit: 'kg/kVA', meaning: 'Select this variant for the lowest model active mass per unit of rated power. This excludes the tank and oil; it is not shipping mass.', short: 'Lowest model active mass per rated kVA' },
    { key: 'noLoadCurrent', title: 'Minimum I₀/I₂', field: 'noLoadCurrentPercent', unit: '%', meaning: 'Select this variant for the smallest no-load current relative to rated LV phase current. A lower ratio does not by itself guarantee the lowest total losses.', short: 'Lowest no-load / rated LV phase current' },
    { key: 'tankVolume', title: 'Minimum tank volume', field: 'tankVolumeM3', unit: 'm³', meaning: 'Select this variant for the smallest calculated tank volume. External cooling tubes and fittings are not included in this volume.', short: 'Smallest calculated tank volume' }
  ];
  const inputs = c => `K ${fmt(c.k)} · Bm ${fmt(c.bm)} T · J ${fmt(c.currentDensityTarget)} A/mm² · L/(D−d) ${fmt(c.windowAspectRatio, 2)}`;
  const action = c => c?.k != null ? `<button type="button" class="soft-button" data-run-variant="${c.serialNumber}">${c.feasible ? 'Run this design' : 'Inspect this diagnostic'} →</button>` : '';
  function findVariant(optimization, serial) {
    return [...Object.values(optimization?.criteriaWinners || {}), ...Object.values(optimization?.calculatedCriteriaWinners || {}), ...(optimization?.candidates || []), ...(optimization?.samples || [])].find(c => c?.serialNumber === Number(serial));
  }
  function replayOverrides(result, serial) {
    const c = findVariant(result?.optimization, serial);
    if (!c?.calculated || c.k == null || !result?.configuration) throw new Error('Run Optimal again to capture the complete configuration for this variant.');
    return { ...result.configuration, EMF_VALUE_FACTOR: c.k, CORE_FLUX_DENSITY: c.bm,
      AVERAGE_CURRENT_DENSITY: c.currentDensityTarget, WINDOW_ASPECT_RATIO: c.windowAspectRatio, AUTOMATIC_CONDUCTOR_SIZING: 1 };
  }
  function table(rows, compact = false) {
    return `<div class="variant-table-wrap"><table class="variant-table${compact ? ' compact' : ''}">
      <thead><tr><th scope="col">Sn</th><th scope="col" class="variant-input">K / Bm / J / H:W</th><th scope="col">Eff.<small>% · 0.85 PF</small></th><th scope="col">Mass<small>kg/kVA</small></th><th scope="col">I₀/I₂<small>%</small></th><th scope="col">Tank<small>m³</small></th><th scope="col">Feasible</th></tr></thead>
      <tbody>${rows.map(c => `<tr class="${c.feasible ? '' : 'variant-excluded'}"><th scope="row">${c.serialNumber}</th><td class="variant-input">${fmt(c.k, 2)} / ${fmt(c.bm, 2)} / ${fmt(c.currentDensityTarget, 2)} / ${fmt(c.windowAspectRatio, 2)}</td><td>${fmt(c.comparisonEfficiencyPercent)}</td><td>${fmt(c.specificMassKgKva)}</td><td>${fmt(c.noLoadCurrentPercent)}</td><td>${fmt(c.tankVolumeM3)}</td><td><span class="${c.feasible ? 'good' : 'bad'}">${c.feasible ? 'Yes' : 'No'}</span>${!c.feasible && !compact ? `<small>${escape(c.constraintFailures)}</small>` : ''}</td></tr>`).join('')}</tbody>
    </table></div>`;
  }
  function winnerCard(criterion, optimization) {
    const c = optimization.criteriaWinners?.[criterion.key] || optimization.calculatedCriteriaWinners?.[criterion.key];
    const fallback = !optimization.criteriaWinners?.[criterion.key];
    return `<article class="criterion-card"><p class="kicker">${criterion.title}</p>${c ? `<h3>${fallback ? 'Diagnostic' : 'Select'} variant <span>${c.serialNumber}</span></h3><div class="criterion-value">${fmt(c[criterion.field], 4)} <small>${criterion.unit}</small></div><p>${fallback ? 'No feasible design was found. This calculated result is available for investigation.' : criterion.short}</p><small>${inputs(c)}</small><strong class="criterion-status ${c.feasible ? 'good' : 'bad'}">${c.feasible ? 'FEASIBLE UNDER SELECTED LIMITS' : 'CALCULATED · NOT FEASIBLE'}</strong>${!c.feasible ? `<small class="criterion-failure">${escape(c.constraintFailures)}</small>` : ''}${action(c)}` : '<h3>No calculated variant</h3><p>The simulator did not return a usable result for this search.</p>'}</article>`;
  }
  function geometryTable(rows) {
    const columns = [['coreDiameterM','d'],['windowHeightM','L'],['centreDistanceM','D'],['yokeLengthM','W'],['actualWindowRatio','L/(D−d)'],['lvCurrentDensity','cdLV'],['hvCurrentDensity','cdHV'],['regulationPercent','Reg %'],['coolingTubes','Nt']];
    return `<details class="constraint-details"><summary>Dimensions and winding results <span>Remaining textbook columns</span></summary><p class="comparison-note">d, L, D and W in metres; cdLV and cdHV in A/mm²; Reg at full load, 0.85 PF. Nt is the number of cooling tubes. These are the same sampled serial numbers.</p><div class="variant-table-wrap"><table class="variant-table"><thead><tr><th>Sn</th>${columns.map(([, label]) => `<th>${label}</th>`).join('')}</tr></thead><tbody>${rows.map(c => `<tr><th>${c.serialNumber}</th>${columns.map(([key]) => `<td>${fmt(c[key], key === 'coolingTubes' ? 0 : 3)}</td>`).join('')}</tr>`).join('')}</tbody></table></div></details>`;
  }
  function balanced(optimization) {
    const c = optimization.candidates?.[optimization.recommendedIndex];
    if (!c) return '<p class="empty-copy">No balanced recommendation: none of the evaluated variants passed every implemented hard constraint.</p>';
    return `<div class="balanced-summary"><div><p class="kicker">BALANCED PARETO CHOICE</p><h3>Select variant ${c.serialNumber}</h3><p>${inputs(c)}</p></div><dl><div><dt>Total loss</dt><dd>${fmt(c.totalLossW / 1000)} kW</dd></div><div><dt>Active mass</dt><dd>${fmt(c.activeMassKg, 1)} kg</dd></div><div><dt>Cost index</dt><dd>${fmt(c.materialCostIndex, 1)}</dd></div><div><dt>Balance score</dt><dd>${fmt(c.balanceScore, 4)}</dd></div></dl></div>`;
  }
  const balanceExplanation = 'Loss, active mass and material cost index are each normalized over the full feasible Pareto set. The smallest equal-weight distance to the ideal point is selected. This is a model-based compromise, not a unique universal optimum.';
  const qualification = 'Feasible means the configured physical-fit and objective limits passed. Kenya Power benchmark compliance is separate; a recommendation is not a manufacturing approval.';
  function render(optimization) {
    if (!optimization.samples) return '<p class="empty-copy">Run Optimal again after updating the backend to generate the variant comparison.</p>';
    return `<header class="comparison-header"><div><p class="kicker">DESIGN VARIANT COMPARISON</p><h2>Choose for your priority</h2></div><button id="presentOptimization" type="button" class="soft-button">Present comparison ↗</button></header>
      <p class="comparison-note">${optimization.samples.length} evenly spaced attempts out of ${optimization.evaluatedDesigns}; ${optimization.calculatedDesigns ?? optimization.evaluatedDesigns} calculated; ${optimization.feasibleDesigns} feasible. Serial numbers are original search IDs, not ranks. Bm in T; J in A/mm²; H/W is the target window ratio.</p>
      ${table(optimization.samples)}
      ${geometryTable(optimization.samples)}
      <p class="comparison-note">Efficiency is at full load and 0.85 PF, using the configured loss reference temperature. kg/kVA uses the C model’s active-mass accounting. The four selections below consider every feasible attempt, including variants outside this 15-row sample.</p>
      <div class="criteria-grid">${criteria.map(c => winnerCard(c, optimization)).join('')}</div>
      <p class="comparison-note">The four textbook selections consider all feasible rows, following core-type §5.2.9–5.2.10. If none are feasible, calculated results are labelled diagnostic. Exact ties use the earliest serial; displayed rounding may hide small differences.</p>
      ${balanced(optimization)}<p class="comparison-note">${balanceExplanation}</p><p class="comparison-note">${qualification}</p>`;
  }
  function section(optimization) {
    if (!optimization?.samples) return null;
    return { id: 'optimization', number: '07', owner: 'Optimal design', title: 'Design variant comparison', source: 'Textbook §5.2.10 · C search results',
      slides: [...Array.from({ length: Math.ceil(optimization.samples.length / 5) }, (_, page) => ({ kind: 'table', page })), ...criteria.map(criterion => ({ kind: 'criterion', criterion })), { kind: 'balance' }] };
  }
  function slide(slide, optimization) {
    if (slide.kind === 'table') {
      const start = slide.page * 5;
      const rows = optimization.samples.slice(start, start + 5);
      return `<p class="kicker">EVENLY SPACED SEARCH SAMPLE · ${start + 1}–${start + rows.length} OF ${optimization.samples.length}</p><h2>Comparing the variants</h2>${table(rows, true)}<p class="comparison-note">Full load · 0.85 PF. Bm [T] / J [A/mm²] / H:W target. “No” excludes a variant from the balanced Pareto recommendation; it is used for a textbook-priority fallback only when no row passes the configured gate, and will be labelled not feasible. ${optimization.feasibleDesigns} of ${optimization.evaluatedDesigns} attempts passed the configured gate.</p>`;
    }
    if (slide.kind === 'criterion') return `<p class="kicker">SELECTION BY DESIGN PRIORITY</p><h2>${slide.criterion.title}</h2>${winnerCard(slide.criterion, optimization)}<p class="comparison-note">${slide.criterion.meaning}</p><p class="comparison-note">The textbook selection is from feasible rows. If the feasible set is empty, this slide presents the best calculated row and states why it is not feasible. Exact ties use the earliest serial. ${qualification}</p>`;
    return `<p class="kicker">THE BALANCED RECOMMENDATION</p><h2>Bringing the trade-offs together</h2>${balanced(optimization)}<p class="comparison-note">${balanceExplanation}</p><p class="comparison-note">${qualification}</p>`;
  }
  return { render, section, slide, findVariant, replayOverrides };
})();
