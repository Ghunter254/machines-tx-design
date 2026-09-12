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

const defaults = {
  APPARENT_POWER: 630, HV_VOLTAGE: 11000, LV_VOLTAGE: 415, FREQUENCY: 50,
  VECTOR_CLOCK: 11, HV_CONNECTION: 'delta', LV_CONNECTION: 'star',
  CORE_FLUX_DENSITY: 1.6, AVERAGE_CURRENT_DENSITY: 2.6,
  WINDOW_ASPECT_RATIO: 3, AUTOMATIC_CONDUCTOR_SIZING: 0
};

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
  return [...document.querySelectorAll('.scope-grid input:checked')].map(input => input.value);
}

function setScope(sectionIds) {
  const all = sectionIds.includes('all');
  document.querySelectorAll('.scope-grid input').forEach(input => { input.checked = all || sectionIds.includes(input.value); });
}

function updateScopeState() {
  document.querySelectorAll('.scope-grid input').forEach(input => { input.disabled = currentMode !== 'explore'; });
}

function overrides() {
  return {
    APPARENT_POWER: $('kva').value, HV_VOLTAGE: $('hv').value, LV_VOLTAGE: $('lv').value,
    FREQUENCY: $('freq').value, VECTOR_CLOCK: $('clock').value,
    HV_CONNECTION: $('hvConnection').value, LV_CONNECTION: $('lvConnection').value,
    CORE_FLUX_DENSITY: $('bm').value, AVERAGE_CURRENT_DENSITY: $('cdav').value,
    WINDOW_ASPECT_RATIO: $('aspect').value,
    AUTOMATIC_CONDUCTOR_SIZING: $('autoSizing').checked ? 1 : 0
  };
}

function populateScenarios() {
  const grouped = {};
  scenarios.forEach(scenario => { (grouped[scenario.group] ||= []).push(scenario); });
  $('scenario').innerHTML = '<option value="manual">Manual input</option>' + Object.entries(grouped).map(([group, items]) =>
    `<optgroup label="${escapeHtml(group)}">${items.map(item => `<option value="${escapeHtml(item.id)}">${escapeHtml(item.name)}</option>`).join('')}</optgroup>`
  ).join('');
}

function selectedScenario() {
  return scenarios.find(scenario => scenario.id === $('scenario').value);
}

function showScenario(scenario) {
  const title = scenario?.name || 'Manual input';
  const summary = scenario?.summary || 'Inputs are controlled directly from the configuration page.';
  const expectation = scenario?.expect || 'The calculation will follow the selected rating, connection and design assumptions.';
  $('scenarioTitle').textContent = title;
  $('scenarioSummary').textContent = summary;
  $('scenarioNote').textContent = title;
  $('scenarioExpect').textContent = expectation;
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

function setMode(mode) {
  currentMode = mode;
  document.querySelectorAll('[data-mode]').forEach(button => {
    const active = button.dataset.mode === mode;
    button.classList.toggle('active', active);
    button.setAttribute('aria-pressed', String(active));
  });
  if (mode === 'nominal' || mode === 'optimize') setScope(['all']);
  updateScopeState();
  if (lastResult) renderDashboardHeader(lastResult);
}

function renderDashboardHeader(data) {
  const input = data.input || {};
  const completed = data.meta?.completedSections || [];
  $('designKicker').textContent = `${String(data.meta?.mode || currentMode).toUpperCase()} DESIGN · ${input.connectionCode || '—'} · ${input.cooling || '—'}`;
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
  $('optimizationSection').hidden = !optimization;
  if (!optimization) return;
  $('candidateCount').textContent = `${optimization.candidates?.length || 0} retained`;
  $('optimizationResults').innerHTML = (optimization.candidates || []).slice(0, 8).map((candidate, index) => `<article class="candidate-item">
    <span class="candidate-rank">${index === optimization.recommendedIndex ? '★' : String(index + 1).padStart(2, '0')}</span>
    <div><strong>Bm ${number(candidate.bm, 3)} T · J ${number(candidate.currentDensityTarget, 3)} A/mm²</strong><small>${number(candidate.totalLossW / 1000, 3)} kW loss · ${number(candidate.activeMassKg, 1)} kg active mass</small></div>
    <span class="candidate-score">${number(candidate.balanceScore, 3)}</span>
  </article>`).join('');
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
    showError('This section was not included in the latest run. Select it from Run scope and calculate again.');
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
  $('engineDot').className = running ? 'running' : engineReady ? '' : 'bad';
  if (running) $('engineStatus').textContent = 'C engine running';
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
  $('metricView').addEventListener('change', () => { renderMetricNote(); if (lastResult) renderHeroMetrics(lastResult); });
  $('runButton').addEventListener('click', runSimulation);
  $('configRunButton').addEventListener('click', runSimulation);
  $('prevSlide').addEventListener('click', () => { if (slideIndex > 0) { slideIndex--; renderDetail(activeSectionId); } });
  $('nextSlide').addEventListener('click', () => { const section = sections.find(item => item.id === activeSectionId); if (section && slideIndex < section.slides.length - 1) { slideIndex++; renderDetail(activeSectionId); } });
  $('slideDots').addEventListener('click', event => { const button = event.target.closest('[data-slide]'); if (!button) return; slideIndex = Number(button.dataset.slide); renderDetail(activeSectionId); });
  window.addEventListener('hashchange', route);
  document.addEventListener('keydown', event => {
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
showScenario(null);
setMode('nominal');
bindEvents();
route();
checkHealth();
loadLatestResult();
