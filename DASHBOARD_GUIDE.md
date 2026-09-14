# TX-C Dashboard Guide

The dashboard is a control surface for the compiled C transformer model. The browser sends the selected mode, configuration values and run scope to the backend. The backend executes the C program, then returns the newly calculated results as JSON.

## Build and run locally

From Ubuntu/WSL:

```bash
cd "/mnt/c/Users/ypaul/Documents/4.1/Electrical Machines/tx-c"
npm run build:c
npm start
```

Open `http://127.0.0.1:5173` in a browser. Confirm that the control rail reports **C engine ready**.

You only need to run `npm run build:c` after changing C source files. Pressing **Run design** does not compile the source again; it runs the existing executable with the current dashboard inputs and recalculates the outputs.

## Using the controls

1. Select a run mode.
2. Open **Configure**. The page changes to match the selected mode.
3. In Nominal, confirm the full reference-design inputs.
4. In Explore, choose the result section first, then change an assumption or apply an optional experiment preset.
5. In Optimal, set the permitted flux-density, current-density and window-ratio ranges.
6. Choose the metric view that matches the question you are investigating.
7. Press **Run design** or use `Ctrl+Enter`.
8. Check the recalculation time and run identifier to confirm that fresh C results were returned.
9. Open any completed section card to see its equations, substitutions, complete outputs and checks.

The configuration note explains the expected direction of change for each preset. It is a hypothesis to compare against the calculated result, not a substitute for the calculation.

## Run modes

### Nominal

Runs the complete calculation chain using the configured transformer rating and assumptions. This is the standard project result.

### Explore

Allows any exposed input to be changed and any combination of result sections to be requested. The engine automatically calculates all prerequisites needed by a requested section. For example, requesting only Tank still calculates the earlier magnetic, winding and performance values required to produce a physically connected tank result.

### Optimal

Searches the configured design space, rejects infeasible candidates and returns a ranked Pareto candidate set. The recommendation is displayed before the reference summary, with its change in loss and active mass. Choose **Explore this candidate** to copy its Bm, current-density and window-ratio values into Explore for a full calculation and presentation.

## Tutorial and presentation mode

The quick-start tutorial opens once in a new browser profile. Choose **Skip tutorial** at any time. The `?` control in the top bar opens it again.

To present a calculation:

1. Run the required section or the full design.
2. Open a completed section card.
3. Choose **Present**.
4. Use the on-screen arrows, keyboard left/right arrows, or Space for the next slide.
5. At the end of a section, Next continues to the next completed project section without leaving presentation mode.
6. Press Escape or the × control to return to the detailed section page.

The complete order is Lenana → Natasha → Aitsa → Stephanie → Hadassah → Yona. After an Optimal run, the new variant-comparison slides follow Yona. In a partial Explore run, the presentation follows only the completed prerequisite chain.

### Presenting the optimal comparison

Run Optimal again after updating both the backend and frontend. At the bottom of the optimal-results panel, **Present comparison** opens eight slides: three pages of five sampled variants, one slide for each of the four textbook criteria, then the balanced Pareto choice. Arrow keys move between slides and sections; Exit or Finish returns to the results.

The 15 rows are evenly spaced attempts from the original 294-item search, including rejected attempts. Serial numbers are IDs, not ranks. A row marked **No** failed the configured feasibility gate; its failure reasons appear in the dashboard table. The four textbook winners normally come from feasible rows, as required by §5.3.9. If the feasible set is empty, the best calculated row is shown as a diagnostic fallback and labelled **CALCULATED · NOT FEASIBLE**. A winner may not appear in the sample, and the same variant may win multiple criteria. Exact ties choose the earliest serial; rounded display values may appear tied even when the underlying values differ.

The comparison efficiency is at full load and **0.85 PF**, matching the textbook optimal-output table. The existing headline efficiency uses unity PF. kg/kVA is the current model's active-mass measure, not shipping mass; see README's existing three-phase copper-accounting limitation. Tank volume excludes external tubes and fittings. All values come from the C results.

The balanced recommendation uses the full feasible Pareto frontier for equal-weight normalized loss, active mass and material-cost ranking. Feasibility uses the limits shown in Optimal configuration and stored in `data/config.txt`: physical non-overlap and non-negative axial fit, actual current-density range, thermal margin, plus minimum efficiency and maximum kg/kVA, I0/I2 and tank volume. The stricter 7 mm axial and 15 mm adjacent-clearance checks remain engineering review results. It does not guarantee Kenya Power benchmark compliance. If no attempt passes the configured gate, the samples remain visible and recommendations explicitly say no feasible variant.

## Test configurations and metric views

`dist/scenarios.js` contains the test configurations and the expected behavior notes. It is the file to edit when adding another comparison case.

The metric selector changes the key values emphasized at the top of the result workspace. It does not change the C calculation. Available views cover:

- design overview;
- electrical behavior;
- losses and efficiency;
- geometry and material use;
- thermal behavior and cooling.

## Results and reports

Every successful run updates:

- `data/output.txt` — readable engineering report;
- `data/output.json` — structured result used by the dashboard;
- the calculation-chain results shown in the browser;
- the checks and interpretation shown below the results.

Use the TXT and JSON controls to inspect the latest backend report directly.

## Frontend and backend deployment

The two surfaces are intentionally separable:

- `dist/` is the static frontend for Vercel.
- `backend/`, `src/`, `include/`, `data/`, the root `Dockerfile` and the compiled C executable form the Render service.

For deployment:

1. Push the repository to GitHub.
2. Create a Render Web Service from `render.yaml` or the root `Dockerfile`.
3. Copy `vercel.json.example` to `vercel.json` and replace the example Render URL with the deployed service URL.
4. Import the repository into Vercel and set `dist` as the static output directory.
5. Set `CORS_ORIGIN` on Render to the Vercel site origin if you do not use the Vercel proxy rewrite.

The Render container compiles the C program during its build. Normal website requests only execute that compiled program.

## Troubleshooting

- **C engine not built:** run `npm run build:c` from Ubuntu/WSL, then restart `npm start`.
- **Backend unreachable:** confirm the backend is running and open `/api/health` on its URL.
- **Old results remain visible:** check the run-state line for the latest update time and confirm the request completed successfully.
- **A partial run includes extra sections:** those sections are prerequisites requested automatically by the engine.
- **A Vercel page loads but Run fails:** check the API rewrite target and the Render service health endpoint.
