const $ = (id) => document.getElementById(id);
const scenarios = window.TX_SCENARIOS || [];
const metricSets = window.TX_METRICS || {};
const presentation = window.TX_PRESENTATION || { sections: [], get: () => undefined };
const sections = presentation.sections;
const get = presentation.get;

let currentMode = 'nominal';
let lastResult = null;
let activeSectionId = null;
let slideIndex = 0;
let engineReady = false;
let exploreScope = ['frame'];
let presentationActive = false;
let tutorialIndex = 0;

const tutorialStorageKey = 'txc-tutorial-complete-v1';

const defaults = {
  APPARENT_POWER: 630, HV_VOLTAGE: 11000, LV_VOLTAGE: 415, FREQUENCY: 50,
  VECTOR_CLOCK: 11, HV_CONNECTION: 'delta', LV_CONNECTION: 'star',
  CORE_FLUX_DENSITY: 1.6, AVERAGE_CURRENT_DENSITY: 2.6,
  WINDOW_ASPECT_RATIO: 3, AUTOMATIC_CONDUCTOR_SIZING: 0,
  OPTIMIZER_BM_MIN: 1.4, OPTIMIZER_BM_MAX: 1.7,
  OPTIMIZER_CURRENT_DENSITY_MIN: 2.3, OPTIMIZER_CURRENT_DENSITY_MAX: 3.2,
  OPTIMIZER_ASPECT_RATIO_MIN: 2.5, OPTIMIZER_ASPECT_RATIO_MAX: 4.0
};

const modeCopy = {
  nominal: {
    kicker: 'NOMINAL INPUT', title: 'Set the reference design',
    description: 'Review the nameplate and assumed values used for the complete textbook calculation chain.',
    run: 'Run full design'
  },
  explore: {
    kicker: 'SECTION STUDY', title: 'Explore selected results',
    description: 'Choose the result section first, then change only the assumptions needed for that experiment. Earlier dependencies are calculated automatically.',
    run: 'Run selected sections'
  },
  optimize: {
    kicker: 'DESIGN SEARCH', title: 'Define the optimization space',
    description: 'Set the transformer duty and practical search limits. The C engine will compare feasible designs and recommend a balanced candidate.',
    run: 'Find optimal design'
  }
};

const tutorialSteps = [
  {
    eyebrow: '01 · Choose an intent', title: 'Three modes, three different jobs',
    body: 'Nominal produces the full reference calculation. Explore studies only the result sections you choose. Optimal searches a practical design space and recommends a Pareto-balanced candidate.',
    visual: '<div class="tutorial-mode-row"><span class="active">Nominal<small>Reference</small></span><span>Explore<small>Section study</small></span><span>Optimal<small>Design search</small></span></div>'
  },
  {
    eyebrow: '02 · Configure', title: 'The form follows the selected mode',
    body: 'Explore begins with a section picker, while Optimal begins with flux-density, current-density and window-ratio limits. Nameplate and connection remain available wherever they affect the calculation.',
    visual: '<div class="tutorial-schematic"><i></i><span>Choose mode</span><b>→</b><i></i><span>Focused inputs</span><b>→</b><i></i><span>C calculation</span></div>'
  },
  {
    eyebrow: '03 · Calculate', title: 'Every run is recalculated by C',
    body: 'Run design sends the current inputs to the backend. The status line changes while the executable runs, and the dashboard records a new time and run identifier when fresh results arrive.',
    visual: '<div class="tutorial-run"><span class="tutorial-engine-dot"></span><strong>C engine running</strong><span>→</span><em>Recalculated 10:52 · run 6761bc</em></div>'
  },
  {
    eyebrow: '04 · Present', title: 'Move through the complete calculation story',
    body: 'Open any completed section, then choose Present. Arrow keys move through formulas and continue automatically from Lenana to Natasha, Aitsa, Stephanie, Hadassah and Yona.',
    visual: '<div class="tutorial-sequence"><span>Lenana</span><b>→</b><span>Natasha</span><b>→</b><span>Aitsa</span><b>→</b><span>…</span><b>→</b><span>Yona</span></div>'
  }
];

const evaluationMap = {
  frame: ['window_ratio', 'kplc_no_load_loss'],
  'no-load': ['no_load_current'],
  lv: ['lv_current_density', 'lv_slack', 'kplc_load_loss'],
  hv: ['hv_current_density', 'hv_slack', 'inter_winding_clearance', 'kplc_load_loss'],
  performance: ['kplc_no_load_loss', 'kplc_load_loss', 'kplc_total_loss', 'kplc_impedance', 'full_load_efficiency'],
  tank: ['temperature_rise', 'oil_volume']
};

const metricEvaluationMap = {
  'summary.ironLossW': 'kplc_no_load_loss',
  'summary.loadLossW': 'kplc_load_loss',
  'summary.totalLossW': 'kplc_total_loss',
  'summary.impedancePercent': 'kplc_impedance',
  'summary.fullLoadEfficiencyPercent': 'full_load_efficiency',
  'summary.temperatureRiseC': 'temperature_rise',
  'sections.noLoad.noLoadCurrentPercent': 'no_load_current',
  'sections.lv.currentDensity': 'lv_current_density',
  'sections.hv.currentDensity': 'hv_current_density',
  'sections.tank.oilVolumeM3': 'oil_volume'
};

const escapeHtml = (value) => String(value ?? '').replace(/[&<>"']/g, character => ({
  '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;'
}[character]));

const number = (value, digits = 3) => Number.isFinite(Number(value))
  ? Number(value).toLocaleString(undefined, { maximumFractionDigits: digits })
  : '—';

const statusClass = (status) => ({ PASS: 'good', INFO: 'neutral', WARN: 'warn', FAIL: 'bad' }[String(status || '').toUpperCase()] || 'neutral');
const api = (path, options) => fetch(`${window.TX_API_BASE || ''}${path}`, options);

function evaluationById(id) {
  return (lastResult?.evaluations || []).find(entry => entry.id === id);
}

function evaluationsForSection(id) {
  const wanted = evaluationMap[id] || [];
  return (lastResult?.evaluations || []).filter(entry => wanted.includes(entry.id));
}

function sectionStatus(id) {
  const values = evaluationsForSection(id);
  if (values.some(entry => entry.status === 'FAIL')) {
    const failures = values.filter(entry => entry.status === 'FAIL').length;
    return { className: 'bad', label: failures === 1 ? '1 check needs review' : `${failures} checks need review` };
  }
  if (values.some(entry => entry.status === 'WARN')) return { className: 'warn', label: 'Close to a limit' };
  if (values.some(entry => entry.status === 'PASS')) return { className: 'good', label: 'Checks passed' };
  return { className: 'neutral', label: 'Calculated result' };
}

function valueFor(definition, data = lastResult) {
  const raw = get(data, definition.path);
  if (!Number.isFinite(Number(raw))) return '—';
  return `${number(Number(raw) * (definition.multiplier ?? 1), definition.digits)}${definition.unit ? ` ${definition.unit}` : ''}`;
}

function setInput(id, value) {
  if ($(id) && value !== undefined && value !== null) $(id).value = value;
}

function resetInputs() {
  setInput('kva', defaults.APPARENT_POWER);
  setInput('hv', defaults.HV_VOLTAGE);
  setInput('lv', defaults.LV_VOLTAGE);
  setInput('freq', defaults.FREQUENCY);
  setInput('clock', defaults.VECTOR_CLOCK);
  setInput('hvConnection', defaults.HV_CONNECTION);
  setInput('lvConnection', defaults.LV_CONNECTION);
  setInput('bm', defaults.CORE_FLUX_DENSITY);
  setInput('cdav', defaults.AVERAGE_CURRENT_DENSITY);
  setInput('aspect', defaults.WINDOW_ASPECT_RATIO);
  setInput('optBmMin', defaults.OPTIMIZER_BM_MIN);
  setInput('optBmMax', defaults.OPTIMIZER_BM_MAX);
  setInput('optCdMin', defaults.OPTIMIZER_CURRENT_DENSITY_MIN);
  setInput('optCdMax', defaults.OPTIMIZER_CURRENT_DENSITY_MAX);
  setInput('optAspectMin', defaults.OPTIMIZER_ASPECT_RATIO_MIN);
  setInput('optAspectMax', defaults.OPTIMIZER_ASPECT_RATIO_MAX);
  $('autoSizing').checked = false;
}

function syncInputsFromResult(data) {
  const input = data?.input;
  if (!input) return;
  setInput('kva', input.kva);
  setInput('hv', input.hvVoltage);
  setInput('lv', input.lvVoltage);
  setInput('freq', input.frequency);
  setInput('clock', input.vectorClock);
  setInput('hvConnection', String(input.hvConnection || '').toLowerCase());
  setInput('lvConnection', String(input.lvConnection || '').toLowerCase());
  setInput('bm', input.coreFluxDensityT);
  setInput('cdav', input.averageCurrentDensity);
  setInput('aspect', input.windowAspectRatio);
}

function selectedSections() {
  if (currentMode !== 'explore') return ['all'];
  return [...document.querySelectorAll('#exploreScope input:checked')].map(input => input.value);
}

function setScope(sectionIds) {
  const all = sectionIds.includes('all');
  document.querySelectorAll('#exploreScope input').forEach(input => { input.checked = all || sectionIds.includes(input.value); });
  const selected = selectedSections();
  if (currentMode === 'explore' && selected.length) exploreScope = selected;
  updateScopeSummary();
}

function updateScopeSummary() {
  const selected = selectedSections();
  if (!selected.length) {
    $('scopeSummary').textContent = 'Choose at least one section. Its prerequisite chain will be included automatically.';
    return;
  }
  const names = selected.map(id => sections.find(section => section.id === id)?.title).filter(Boolean);
  const furthest = Math.max(...selected.map(id => sections.findIndex(section => section.id === id)));
  const prerequisites = sections.slice(0, furthest + 1).map(section => section.owner);
  $('scopeSummary').textContent = `${names.join(' + ')} selected. The engine will calculate the required chain through ${prerequisites.at(-1)} (${prerequisites.join(' → ')}).`;
}

function overrides() {
  return {
    APPARENT_POWER: $('kva').value, HV_VOLTAGE: $('hv').value, LV_VOLTAGE: $('lv').value,
    FREQUENCY: $('freq').value, VECTOR_CLOCK: $('clock').value,
    HV_CONNECTION: $('hvConnection').value, LV_CONNECTION: $('lvConnection').value,
    CORE_FLUX_DENSITY: $('bm').value, AVERAGE_CURRENT_DENSITY: $('cdav').value,
    WINDOW_ASPECT_RATIO: $('aspect').value,
    AUTOMATIC_CONDUCTOR_SIZING: $('autoSizing').checked ? 1 : 0,
    OPTIMIZER_BM_MIN: $('optBmMin').value, OPTIMIZER_BM_MAX: $('optBmMax').value,
    OPTIMIZER_CURRENT_DENSITY_MIN: $('optCdMin').value, OPTIMIZER_CURRENT_DENSITY_MAX: $('optCdMax').value,
    OPTIMIZER_ASPECT_RATIO_MIN: $('optAspectMin').value, OPTIMIZER_ASPECT_RATIO_MAX: $('optAspectMax').value
  };
}

function populateScenarios() {
  const grouped = {};
  scenarios.forEach(scenario => { (grouped[scenario.group] ||= []).push(scenario); });
  $('scenario').innerHTML = '<option value="manual">Current design values</option>' + Object.entries(grouped).map(([group, items]) =>
    `<optgroup label="${escapeHtml(group)}">${items.map(item => `<option value="${escapeHtml(item.id)}">${escapeHtml(item.name)}</option>`).join('')}</optgroup>`
  ).join('');
}

function selectedScenario() {
  return scenarios.find(scenario => scenario.id === $('scenario').value);
}

function showScenario(scenario) {
  const title = scenario?.name || 'Current design values';
  const summary = scenario?.summary || 'No prepared comparison is applied.';
  const expectation = scenario?.expect || 'The calculation will follow the selected rating, connection and design assumptions.';
  $('scenarioNote').textContent = title;
  $('scenarioExpect').textContent = expectation;
  updateDashboardContext();
}

function applyScenario() {
  const scenario = selectedScenario();
  if (!scenario) { showScenario(null); return; }
  resetInputs();
  const fieldMap = {
    APPARENT_POWER: 'kva', HV_VOLTAGE: 'hv', LV_VOLTAGE: 'lv', FREQUENCY: 'freq',
    VECTOR_CLOCK: 'clock', HV_CONNECTION: 'hvConnection', LV_CONNECTION: 'lvConnection',
    CORE_FLUX_DENSITY: 'bm', AVERAGE_CURRENT_DENSITY: 'cdav', WINDOW_ASPECT_RATIO: 'aspect',
    AUTOMATIC_CONDUCTOR_SIZING: 'autoSizing'
  };
  Object.entries(scenario.changes || {}).forEach(([key, value]) => {
    const field = fieldMap[key];
    if (!field) return;
    if (field === 'autoSizing') $('autoSizing').checked = Boolean(Number(value));
    else setInput(field, value);
  });
  if (scenario.sections) setScope(scenario.sections);
  showScenario(scenario);
}

function populateMetrics() {
  $('metricView').innerHTML = Object.entries(metricSets).map(([id, set]) => `<option value="${escapeHtml(id)}">${escapeHtml(set.label)}</option>`).join('');
  renderMetricNote();
}

function renderMetricNote() {
  const set = metricSets[$('metricView').value] || metricSets.overview;
  $('metricNote').textContent = set?.description || '';
}

function renderConfigMode() {
  const copy = modeCopy[currentMode];
  $('configKicker').textContent = copy.kicker;
  $('configTitle').textContent = copy.title;
  $('configDescription').textContent = copy.description;
  $('configRunLabel').textContent = copy.run;
  document.querySelectorAll('[data-config-modes]').forEach(card => {
    card.hidden = !card.dataset.configModes.split(' ').includes(currentMode);
  });
  [...document.querySelectorAll('.config-card:not([hidden])')].forEach((card, index) => {
    const numberElement = card.querySelector('.config-card-head > span');
    if (numberElement) numberElement.textContent = String(index + 1).padStart(2, '0');
  });
  if (currentMode === 'explore') {
    setScope(exploreScope.length ? exploreScope : ['frame']);
  }
}

function updateDashboardContext() {
  if (currentMode === 'optimize') {
    $('contextLabel').textContent = 'Search strategy';
    $('scenarioTitle').textContent = 'Balanced Pareto search';
    $('scenarioSummary').textContent = 'Compares feasible candidates across loss, active mass and relative material cost.';
    return;
  }
  if (currentMode === 'explore') {
    const selected = selectedSections();
    const labels = selected.map(id => sections.find(section => section.id === id)?.owner).filter(Boolean);
    const scenario = selectedScenario();
    $('contextLabel').textContent = 'Explore target';
    $('scenarioTitle').textContent = labels.length ? labels.join(' + ') : 'Choose a section';
    $('scenarioSummary').textContent = scenario
      ? `${scenario.name}: ${scenario.summary}`
      : labels.length ? 'Only the selected result story is requested; prerequisites are resolved automatically.' : 'Open Configure and choose the result you want to investigate.';
    return;
  }
  $('contextLabel').textContent = 'Design setup';
  $('scenarioTitle').textContent = 'Configured reference design';
  $('scenarioSummary').textContent = 'Runs every textbook section from magnetic frame through tank and cooling.';
}

function setMode(mode) {
  if (!modeCopy[mode]) return;
  if (currentMode === 'explore') {
    const remembered = selectedSections();
    if (remembered.length) exploreScope = remembered;
  }
  currentMode = mode;
  document.querySelectorAll('[data-mode]').forEach(button => {
    const active = button.dataset.mode === mode;
    button.classList.toggle('active', active);
    button.setAttribute('aria-pressed', String(active));
  });
  renderConfigMode();
  updateDashboardContext();
  if (lastResult) {
    renderDashboardHeader(lastResult);
    renderOptimization(lastResult);
  }
}

function renderDashboardHeader(data) {
  const input = data.input || {};
  const completed = data.meta?.completedSections || [];
  const modeLabel = { nominal: 'NOMINAL', explore: 'EXPLORE', optimize: 'OPTIMAL' }[currentMode];
  $('designKicker').textContent = `${modeLabel} DESIGN · ${input.connectionCode || '—'} · ${input.cooling || '—'}`;
  $('designTitle').textContent = `${number(input.kva, 0)} kVA transformer`;
  $('designSubtitle').textContent = `${number(input.hvVoltage, 0)} V high voltage · ${number(input.lvVoltage, 0)} V low voltage · ${number(input.frequency, 0)} Hz`;
  $('runStateLabel').textContent = data.meta?.benchmark || 'Calculation state';
  $('runStateTitle').textContent = completed.length === 6 ? 'Complete calculation chain' : `${completed.length} sections calculated`;
}

function metricState(path) {
  const evaluation = evaluationById(metricEvaluationMap[path]);
  if (!evaluation) return { className: 'neutral', label: 'Calculated result' };
  return { className: statusClass(evaluation.status), label: evaluation.status === 'PASS' ? 'Within target' : evaluation.status === 'FAIL' ? 'Review required' : evaluation.status === 'WARN' ? 'Close to limit' : 'Calculated result' };
}

function renderHeroMetrics(data) {
  const set = metricSets[$('metricView').value] || metricSets.overview;
  const metrics = (set?.metrics || []).slice(0, 4);
  $('heroMetrics').innerHTML = metrics.map(([path, label, unit, transform]) => {
    const raw = get(data, path);
    const transformed = raw === undefined ? undefined : transform(raw);
    const state = metricState(path);
    return `<article><span>${escapeHtml(label)}</span><strong>${number(transformed, 3)} <small>${escapeHtml(unit)}</small></strong><em class="${state.className}">${escapeHtml(state.label)}</em></article>`;
  }).join('');
}

function renderSectionCards(data) {
  const completed = data.meta?.completedSections || [];
  $('sectionGrid').innerHTML = sections.map(section => {
    const ready = completed.includes(section.id);
    const state = ready ? sectionStatus(section.id) : { className: 'neutral', label: 'Not included in this run' };
    const href = ready ? `#/section/${section.id}` : '#/configure';
    return `<a class="section-card accent-${section.accent}${ready ? '' : ' pending'}" href="${href}" ${ready ? '' : 'aria-disabled="true"'}>
      <div class="card-gloss"></div><span class="section-number">${section.number}</span><span class="owner">${escapeHtml(section.owner)}</span>
      <h3>${escapeHtml(section.title)}</h3><p>${escapeHtml(section.short)}</p>
      <dl>${section.summary.map(item => `<div><dt>${escapeHtml(item.label)}</dt><dd>${escapeHtml(valueFor(item, data))}</dd></div>`).join('')}</dl>
      <footer><span class="card-state ${state.className}">${escapeHtml(state.label)}</span><span class="open-arrow">↗</span></footer>
    </a>`;
  }).join('');
}

function renderReview(data) {
  const values = data.evaluations || [];
  const passed = values.filter(entry => entry.status === 'PASS').length;
  $('reviewScore').textContent = `${passed}/${values.length}`;
  $('reviewSummary').innerHTML = values.length ? values.map(entry => `<div class="review-item">
    <i class="status-mark ${statusClass(entry.status)}"></i><strong>${escapeHtml(entry.label)}</strong><span>${number(entry.value, 3)} ${escapeHtml(entry.unit)}</span>
  </div>`).join('') : '<p class="empty-copy">Engineering checks appear after a successful calculation.</p>';
}

function renderOptimization(data) {
  const optimization = data.optimization;
  $('optimizationSection').hidden = currentMode !== 'optimize' || !optimization;
  if (currentMode !== 'optimize' || !optimization) return;
  const candidates = optimization.candidates || [];
  const recommended = candidates[optimization.recommendedIndex ?? 0] || candidates[0];
  $('candidateCount').textContent = `${candidates.length} retained`;
  if (recommended) {
    const baselineLoss = Number(data.summary?.totalLossW);
    const baselineMass = Number(data.sections?.tank?.activeMassKg);
    const lossChange = Number.isFinite(baselineLoss) && baselineLoss ? (recommended.totalLossW / baselineLoss - 1) * 100 : undefined;
    const massChange = Number.isFinite(baselineMass) && baselineMass ? (recommended.activeMassKg / baselineMass - 1) * 100 : undefined;
    const signed = value => Number.isFinite(value) ? `${value > 0 ? '+' : ''}${number(value, 1)}%` : '—';
    $('optimizationLead').innerHTML = `<div class="recommendation-copy"><span>RECOMMENDATION${recommended.serialNumber ? ` · VARIANT ${recommended.serialNumber}` : ''}</span><h3>Bm ${number(recommended.bm, 3)} T · J ${number(recommended.currentDensityTarget, 3)} A/mm² · H/W ${number(recommended.windowAspectRatio, 2)}</h3><p>${optimization.paretoCount != null ? `This has the lowest equal-weight normalized loss, mass and cost score across all ${optimization.paretoCount} feasible Pareto variants.` : 'This saved result uses the earlier ranking. Run Optimal again for the updated comparison.'}</p></div>
      <dl class="recommendation-values">
        <div><dt>Total loss</dt><dd>${number(recommended.totalLossW / 1000, 3)} kW <small>${signed(lossChange)} vs reference</small></dd></div>
        <div><dt>Active mass</dt><dd>${number(recommended.activeMassKg, 1)} kg <small>${signed(massChange)} vs reference</small></dd></div>
        <div><dt>Efficiency</dt><dd>${number(recommended.efficiencyPercent, 3)}% <small>${number(recommended.impedancePercent, 3)}% impedance</small></dd></div>
      </dl>
      <button id="applyRecommended" class="apply-recommendation" type="button">Explore this candidate <span>→</span></button>`;
  } else {
    $('optimizationLead').innerHTML = '<p class="empty-copy">No feasible recommendation was returned for this search space.</p>';
  }
  $('optimizationResults').innerHTML = candidates.slice(0, 8).map((candidate, index) => `<article class="candidate-item${candidate === recommended ? ' recommended' : ''}">
    <span class="candidate-rank">${index === optimization.recommendedIndex ? '★' : String(index + 1).padStart(2, '0')}</span>
    <div><strong>Bm ${number(candidate.bm, 3)} T · J ${number(candidate.currentDensityTarget, 3)} A/mm²</strong><small>H/W ${number(candidate.windowAspectRatio, 2)} · ${number(candidate.totalLossW / 1000, 3)} kW · ${number(candidate.activeMassKg, 1)} kg</small></div>
    <span class="candidate-score">${number(candidate.balanceScore, 3)}</span>
  </article>`).join('');
  $('optimizationComparison').innerHTML = window.TX_OPTIMIZATION.render(optimization);
}

function applyRecommendedCandidate() {
  const optimization = lastResult?.optimization;
  const candidates = optimization?.candidates || [];
  const recommended = candidates[optimization?.recommendedIndex ?? 0] || candidates[0];
  if (!recommended) return;
  setInput('bm', recommended.bm);
  setInput('cdav', recommended.currentDensityTarget);
  setInput('aspect', recommended.windowAspectRatio);
  $('autoSizing').checked = true;
  $('scenario').value = 'manual';
  showScenario(null);
  exploreScope = sections.map(section => section.id);
  setMode('explore');
  location.hash = '#/configure';
}

function renderResult(data, syncInputs = false) {
  lastResult = data;
  if (syncInputs) syncInputsFromResult(data);
  renderDashboardHeader(data);
  renderHeroMetrics(data);
  renderSectionCards(data);
  renderReview(data);
  renderOptimization(data);
  if (activeSectionId) renderDetail(activeSectionId);
}

function rangeMarkup(slide) {
  const evaluation = slide.evaluationId ? evaluationById(slide.evaluationId) : null;
  const state = evaluation ? statusClass(evaluation.status) : 'neutral';
  const verdict = evaluation?.verdict || slide.range;
  const label = evaluation?.status || 'BASIS';
  return `<div class="slide-range"><strong class="${state}">${escapeHtml(label)}</strong><p>${escapeHtml(verdict)}</p></div>`;
}

function renderFormulaSlide(section) {
  const slides = section.slides || [];
  const current = slides[slideIndex];
  if (!current) return;
  const raw = get(lastResult, current.resultPath);
  const value = Number.isFinite(Number(raw)) ? Number(raw) * (current.multiplier ?? 1) : undefined;
  $('slideSource').textContent = section.source;
  $('slideCount').textContent = `${slideIndex + 1} / ${slides.length}`;
  $('prevSlide').disabled = slideIndex === 0;
  $('nextSlide').disabled = slideIndex === slides.length - 1;
  $('formulaSlide').innerHTML = `<div class="slide-topline"><span class="slide-symbol">${escapeHtml(current.symbol)}</span><span>CALCULATION ${String(slideIndex + 1).padStart(2, '0')}</span></div>
    <h2>${escapeHtml(current.title)}</h2>
    <div class="formula-display">${escapeHtml(current.formula)}</div>
    <div class="substitution-grid">
      <div class="substitution"><span>VALUE SUBSTITUTION</span><pre>${escapeHtml(current.substitution(lastResult))}</pre></div>
      <div class="slide-result"><span>RESULT</span><strong>${number(value, current.digits)}</strong><small>${escapeHtml(current.unit)}</small></div>
    </div>
    <div class="slide-interpretation"><div><h3>What this value means</h3><p>${escapeHtml(current.meaning)}</p></div>${rangeMarkup(current)}</div>
    ${current.note ? `<p class="slide-note">${escapeHtml(current.note)}</p>` : ''}`;
  $('slideDots').innerHTML = slides.map((entry, index) => `<button type="button" class="${index === slideIndex ? 'active' : ''}" data-slide="${index}" aria-label="Show ${escapeHtml(entry.title)}"></button>`).join('');
}

function availablePresentationSections() {
  const completed = lastResult?.meta?.completedSections || [];
  const available = sections.filter(section => completed.includes(section.id));
  const optimal = window.TX_OPTIMIZATION.section(lastResult?.optimization);
  if (optimal) available.push(optimal);
  return available;
}

function presentationPosition() {
  const available = availablePresentationSections();
  const sectionIndex = available.findIndex(section => section.id === activeSectionId);
  return { available, sectionIndex, section: available[sectionIndex] };
}

function renderPresentationSlide() {
  const { available, sectionIndex, section } = presentationPosition();
  const current = section?.slides?.[slideIndex];
  if (!current) return;
  const raw = current.resultPath ? get(lastResult, current.resultPath) : undefined;
  const value = Number.isFinite(Number(raw)) ? Number(raw) * (current.multiplier ?? 1) : undefined;
  const evaluation = current.evaluationId ? evaluationById(current.evaluationId) : null;
  const rangeLabel = evaluation?.status || 'BASIS';
  const rangeState = evaluation ? statusClass(evaluation.status) : 'neutral';
  const rangeText = evaluation?.verdict || current.range;
  const totalSlides = available.reduce((sum, item) => sum + item.slides.length, 0);
  const absoluteSlide = available.slice(0, sectionIndex).reduce((sum, item) => sum + item.slides.length, 0) + slideIndex + 1;
  const previousSection = available[sectionIndex - 1];
  const nextSection = available[sectionIndex + 1];
  const isFirst = sectionIndex === 0 && slideIndex === 0;
  const isLast = sectionIndex === available.length - 1 && slideIndex === section.slides.length - 1;

  $('presentSectionMeta').textContent = `${section.number} · ${section.owner} · ${section.source}`;
  $('presentSectionTitle').textContent = section.title;
  $('presentSlideCount').textContent = `${absoluteSlide} / ${totalSlides}`;
  $('presentPrev').disabled = isFirst;
  $('presentPrevLabel').textContent = slideIndex === 0 && previousSection ? previousSection.owner : 'Previous';
  $('presentNextLabel').textContent = isLast ? 'Finish' : slideIndex === section.slides.length - 1 && nextSection ? `Continue to ${nextSection.owner}` : 'Next';
  $('presentProgress').innerHTML = available.map((item, index) => `<i class="${index < sectionIndex ? 'done' : index === sectionIndex ? 'active' : ''}" title="${escapeHtml(item.owner)}"></i>`).join('');
  $('presentSlide').classList.toggle('optimal-slide', section.id === 'optimization');
  if (section.id === 'optimization') {
    $('presentSlide').innerHTML = window.TX_OPTIMIZATION.slide(current, lastResult.optimization);
    return;
  }
  $('presentSlide').innerHTML = `<div class="present-topline"><span class="slide-symbol">${escapeHtml(current.symbol)}</span><span>CALCULATION ${String(slideIndex + 1).padStart(2, '0')} · ${escapeHtml(section.owner.toUpperCase())}</span></div>
    <h2>${escapeHtml(current.title)}</h2>
    <div class="present-formula">${escapeHtml(current.formula)}</div>
    <div class="present-work">
      <div class="present-substitution"><span>VALUE SUBSTITUTION</span><pre>${escapeHtml(current.substitution(lastResult))}</pre></div>
      <div class="present-result"><span>RESULT</span><strong>${number(value, current.digits)}</strong><small>${escapeHtml(current.unit)}</small></div>
    </div>
    <div class="present-explanation"><div><strong>What this value means</strong><p>${escapeHtml(current.meaning)}</p></div><div class="present-basis"><strong class="${rangeState}">${escapeHtml(rangeLabel)}</strong><p>${escapeHtml(rangeText)}</p></div></div>
    ${current.note ? `<p class="present-note">${escapeHtml(current.note)}</p>` : ''}`;
}

function movePresentation(direction) {
  const { available, sectionIndex, section } = presentationPosition();
  if (!section) return;
  if (direction > 0) {
    if (slideIndex < section.slides.length - 1) slideIndex += 1;
    else if (sectionIndex < available.length - 1) {
      activeSectionId = available[sectionIndex + 1].id;
      slideIndex = 0;
    } else {
      closePresentation();
      return;
    }
  } else if (slideIndex > 0) slideIndex -= 1;
  else if (sectionIndex > 0) {
    activeSectionId = available[sectionIndex - 1].id;
    slideIndex = available[sectionIndex - 1].slides.length - 1;
  }
  renderPresentationSlide();
}

async function openPresentation() {
  if (!activeSectionId || !lastResult) return;
  presentationActive = true;
  $('presentationMode').hidden = false;
  document.body.classList.add('presenting');
  renderPresentationSlide();
  try {
    if (!$('presentationMode').matches(':fullscreen')) await $('presentationMode').requestFullscreen?.();
  } catch {
    // The fixed overlay remains a full-window presentation if browser fullscreen is denied.
  }
}

async function closePresentation(skipFullscreenExit = false) {
  if (!presentationActive) return;
  presentationActive = false;
  $('presentationMode').hidden = true;
  document.body.classList.remove('presenting');
  if (!skipFullscreenExit && document.fullscreenElement) {
    try { await document.exitFullscreen(); } catch { /* already exiting */ }
  }
  if (activeSectionId === 'optimization') {
    activeSectionId = null;
    history.replaceState(null, '', '#/');
    showView('dashboardView');
    $('optimizationComparison').scrollIntoView({ block: 'start' });
    $('presentOptimization')?.focus({ preventScroll: true });
  } else if (activeSectionId) {
    history.replaceState(null, '', `#/section/${activeSectionId}`);
    showView('detailView');
    renderDetail(activeSectionId);
  }
}

function renderTutorial() {
  const step = tutorialSteps[tutorialIndex];
  $('tutorialStepLabel').textContent = `QUICK START · ${tutorialIndex + 1}/${tutorialSteps.length}`;
  $('tutorialContent').innerHTML = `<p class="tutorial-eyebrow">${escapeHtml(step.eyebrow)}</p><h2 id="tutorialTitle">${escapeHtml(step.title)}</h2><p class="tutorial-body">${escapeHtml(step.body)}</p>${step.visual}`;
  $('tutorialPrev').disabled = tutorialIndex === 0;
  $('tutorialNext').innerHTML = tutorialIndex === tutorialSteps.length - 1 ? '<span>Start designing</span><span>→</span>' : '<span>Next</span><span>→</span>';
  $('tutorialDots').innerHTML = tutorialSteps.map((_, index) => `<i class="${index === tutorialIndex ? 'active' : ''}"></i>`).join('');
}

function openTutorial() {
  tutorialIndex = 0;
  $('tutorialOverlay').hidden = false;
  document.body.classList.add('tutorial-open');
  renderTutorial();
}

function closeTutorial() {
  $('tutorialOverlay').hidden = true;
  document.body.classList.remove('tutorial-open');
  try { localStorage.setItem(tutorialStorageKey, '1'); } catch { /* storage can be disabled */ }
}

function nextTutorialStep() {
  if (tutorialIndex < tutorialSteps.length - 1) {
    tutorialIndex += 1;
    renderTutorial();
  } else closeTutorial();
}

function renderPerformanceCases(section) {
  const cases = get(lastResult, 'sections.performance.cases') || [];
  const visible = section.id === 'performance' && cases.length;
  $('performanceCases').hidden = !visible;
  if (!visible) return;
  $('caseRows').innerHTML = cases.map(item => `<tr>
    <td data-label="Power factor">${number(item.powerFactor, 2)}</td><td data-label="Load">${number(item.loadPu, 2)} p.u.</td>
    <td data-label="Loss">${number(item.lossKw, 4)} kW</td><td data-label="Output">${number(item.outputKw, 2)} kW</td>
    <td data-label="Input">${number(item.inputKw, 2)} kW</td><td data-label="Efficiency">${number(item.efficiencyPercent, 4)}%</td>
  </tr>`).join('');
}

function renderAllOutputs(section) {
  $('outputCount').textContent = `${section.outputs.length} values`;
  $('allOutputs').innerHTML = section.outputs.map(item => `<div class="output-row"><div class="output-label"><strong>${escapeHtml(item.label)}</strong><span>${escapeHtml(item.symbol)}</span></div><div class="output-value">${escapeHtml(valueFor(item))}</div></div>`).join('');
}

function renderSectionChecks(section) {
  const values = evaluationsForSection(section.id);
  $('sectionChecks').innerHTML = values.length ? values.map(entry => `<article class="check-row">
    <span class="check-chip ${statusClass(entry.status)}">${escapeHtml(entry.status)}</span>
    <div class="check-copy"><strong>${escapeHtml(entry.label)}</strong><span>${escapeHtml(entry.meaning)} ${escapeHtml(entry.verdict)}</span></div>
    <span class="check-value">${number(entry.value, 3)} ${escapeHtml(entry.unit)}</span>
  </article>`).join('') : '<p class="empty-copy">This section has calculated outputs but no standalone acceptance rule.</p>';
}

function renderDetail(id) {
  const section = sections.find(item => item.id === id);
  if (!section || !lastResult) return;
  const completed = lastResult.meta?.completedSections || [];
  if (!completed.includes(id)) {
    showError('This section was not included in the latest run. Choose it in Explore configuration and calculate again.');
    location.hash = '#/configure';
    return;
  }
  activeSectionId = id;
  const state = sectionStatus(id);
  const accentValues = { blue: '121, 168, 255', violet: '166, 126, 255', cyan: '83, 207, 225', indigo: '109, 129, 255', rose: '242, 116, 154', green: '92, 211, 150' };
  $('detailHero').style.setProperty('--accent', accentValues[section.accent]);
  $('detailKicker').textContent = `${section.number} · ${section.owner.toUpperCase()} · ${state.label.toUpperCase()}`;
  $('detailTitle').textContent = section.title;
  $('detailSubtitle').textContent = section.short;
  $('detailHeroMetrics').innerHTML = section.summary.map(item => `<div class="hero-result"><span>${escapeHtml(item.label)}</span><strong>${escapeHtml(valueFor(item))}</strong><small>${escapeHtml(item.symbol)}</small></div>`).join('');
  renderFormulaSlide(section);
  renderPerformanceCases(section);
  renderAllOutputs(section);
  renderSectionChecks(section);
}

function showView(view) {
  ['dashboardView', 'detailView', 'configView'].forEach(id => {
    const visible = id === view;
    $(id).hidden = !visible;
    $(id).classList.toggle('view-enter', visible);
  });
}

function route() {
  const hash = location.hash || '#/';
  activeSectionId = null;
  if (hash === '#/configure') showView('configView');
  else if (hash.startsWith('#/section/')) {
    const id = hash.split('/')[2];
    showView('detailView');
    slideIndex = 0;
    activeSectionId = id;
    renderDetail(id);
  } else showView('dashboardView');
  window.scrollTo({ top: 0, behavior: 'auto' });
}

function showError(message) {
  $('errorToast').textContent = message;
  $('errorToast').hidden = false;
  clearTimeout(showError.timer);
  showError.timer = setTimeout(() => { $('errorToast').hidden = true; }, 7000);
}

function setRunning(running) {
  [$('runButton'), $('configRunButton')].forEach(button => { button.disabled = running; });
  $('runLabel').textContent = running ? 'Calculating…' : 'Run design';
  $('configRunLabel').textContent = running ? 'Calculating…' : modeCopy[currentMode].run;
  $('engineDot').className = running ? 'running' : engineReady ? '' : 'bad';
  if (running) $('engineStatus').textContent = 'C engine running';
}

function validateRunRequest() {
  if (currentMode === 'explore' && selectedSections().length === 0) {
    throw new Error('Choose at least one result section in Explore before running.');
  }
  if (currentMode === 'optimize') {
    const ranges = [
      ['Flux density', $('optBmMin').valueAsNumber, $('optBmMax').valueAsNumber],
      ['Current density', $('optCdMin').valueAsNumber, $('optCdMax').valueAsNumber],
      ['Window ratio', $('optAspectMin').valueAsNumber, $('optAspectMax').valueAsNumber]
    ];
    const invalid = ranges.find(([, minimum, maximum]) => !Number.isFinite(minimum) || !Number.isFinite(maximum) || minimum >= maximum);
    if (invalid) throw new Error(`${invalid[0]} minimum must be lower than its maximum.`);
  }
}

async function checkHealth() {
  try {
    const response = await api('/api/health');
    const health = await response.json();
    engineReady = Boolean(health.executable);
    $('engineDot').className = engineReady ? '' : 'bad';
    $('engineStatus').textContent = engineReady ? 'Engine ready' : 'Engine not built';
  } catch {
    engineReady = false;
    $('engineDot').className = 'bad';
    $('engineStatus').textContent = 'Backend offline';
  }
}

async function loadLatestResult() {
  try {
    const response = await api('/api/output.json');
    if (!response.ok) return;
    const data = await response.json();
    renderResult(data, true);
  } catch {
    $('runStateTitle').textContent = 'No saved result available';
    $('designSubtitle').textContent = 'Configure the design and run the C simulation.';
  }
}

async function runSimulation() {
  try {
    validateRunRequest();
  } catch (error) {
    showError(error.message);
    location.hash = '#/configure';
    return;
  }
  setRunning(true);
  $('errorToast').hidden = true;
  try {
    const response = await api('/api/simulate', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mode: currentMode, sections: selectedSections(), overrides: overrides() })
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || 'The simulation request failed.');
    renderResult(result);
    const runTag = String(result.meta?.runId || '').split('-').at(-1).slice(0, 6);
    const runTime = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    $('lastRun').textContent = `Recalculated ${runTime}${runTag ? ` · run ${runTag}` : ''}`;
    location.hash = '#/';
  } catch (error) {
    showError(`${error.message} Confirm that the backend and compiled C engine are available.`);
  } finally {
    await checkHealth();
    setRunning(false);
  }
}

function bindEvents() {
  document.querySelectorAll('[data-mode]').forEach(button => button.addEventListener('click', () => setMode(button.dataset.mode)));
  $('scenario').addEventListener('change', applyScenario);
  document.querySelectorAll('#exploreScope input').forEach(input => input.addEventListener('change', () => {
    exploreScope = selectedSections();
    updateScopeSummary();
    updateDashboardContext();
  }));
  $('metricView').addEventListener('change', () => { renderMetricNote(); if (lastResult) renderHeroMetrics(lastResult); });
  $('runButton').addEventListener('click', runSimulation);
  $('configRunButton').addEventListener('click', runSimulation);
  $('optimizationSection').addEventListener('click', event => {
    if (event.target.closest('#applyRecommended')) applyRecommendedCandidate();
    if (event.target.closest('#presentOptimization')) {
      activeSectionId = 'optimization';
      slideIndex = 0;
      openPresentation();
    }
  });
  $('prevSlide').addEventListener('click', () => { if (slideIndex > 0) { slideIndex--; renderDetail(activeSectionId); } });
  $('nextSlide').addEventListener('click', () => { const section = sections.find(item => item.id === activeSectionId); if (section && slideIndex < section.slides.length - 1) { slideIndex++; renderDetail(activeSectionId); } });
  $('slideDots').addEventListener('click', event => { const button = event.target.closest('[data-slide]'); if (!button) return; slideIndex = Number(button.dataset.slide); renderDetail(activeSectionId); });
  $('presentButton').addEventListener('click', openPresentation);
  $('closePresentation').addEventListener('click', () => closePresentation());
  $('presentPrev').addEventListener('click', () => movePresentation(-1));
  $('presentNext').addEventListener('click', () => movePresentation(1));
  document.addEventListener('fullscreenchange', () => {
    if (presentationActive && !document.fullscreenElement) closePresentation(true);
  });
  $('tutorialButton').addEventListener('click', openTutorial);
  $('skipTutorial').addEventListener('click', closeTutorial);
  $('tutorialPrev').addEventListener('click', () => { if (tutorialIndex > 0) { tutorialIndex -= 1; renderTutorial(); } });
  $('tutorialNext').addEventListener('click', nextTutorialStep);
  window.addEventListener('hashchange', route);
  document.addEventListener('keydown', event => {
    if (presentationActive) {
      if (event.key === 'ArrowLeft') { event.preventDefault(); movePresentation(-1); }
      if (event.key === 'ArrowRight' || event.key === ' ') { event.preventDefault(); movePresentation(1); }
      if (event.key === 'Escape' && !document.fullscreenElement) closePresentation(true);
      return;
    }
    if (!$('tutorialOverlay').hidden) {
      if (event.key === 'ArrowLeft' && tutorialIndex > 0) { tutorialIndex -= 1; renderTutorial(); }
      if (event.key === 'ArrowRight') nextTutorialStep();
      if (event.key === 'Escape') closeTutorial();
      return;
    }
    const tag = document.activeElement?.tagName;
    if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') runSimulation();
    if (activeSectionId && !['INPUT', 'SELECT', 'TEXTAREA'].includes(tag)) {
      if (event.key === 'ArrowLeft' && slideIndex > 0) { slideIndex--; renderDetail(activeSectionId); }
      if (event.key === 'ArrowRight') { const section = sections.find(item => item.id === activeSectionId); if (slideIndex < section.slides.length - 1) { slideIndex++; renderDetail(activeSectionId); } }
    }
  });
}

populateScenarios();
populateMetrics();
resetInputs();
showScenario(null);
setMode('nominal');
bindEvents();
route();
checkHealth();
loadLatestResult();
try { if (!localStorage.getItem(tutorialStorageKey)) openTutorial(); } catch { openTutorial(); }
