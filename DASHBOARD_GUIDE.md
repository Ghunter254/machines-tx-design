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

You only need to run `npm run build:c` after changing C source files. Pressing **Run simulation** does not compile the source again; it runs the existing executable with the current dashboard inputs and recalculates the outputs.

## Using the controls

1. Select a run mode.
2. Choose one of the test configurations, or select **Manual input**.
3. Open **Electrical configuration** or **Design assumptions** to change values.
4. In Explore mode, open **Run scope** and select the sections you want to inspect.
5. Choose the metric view that matches the question you are investigating.
6. Press **Run simulation** or use `Ctrl+Enter`.
7. Read the result chain first, then review the engineering checks and their verdicts.

The configuration note explains the expected direction of change for each preset. It is a hypothesis to compare against the calculated result, not a substitute for the calculation.

## Run modes

### Nominal

Runs the complete calculation chain using the configured transformer rating and assumptions. This is the standard project result.

### Explore

Allows any exposed input to be changed and any combination of result sections to be requested. The engine automatically calculates all prerequisites needed by a requested section. For example, requesting only Tank still calculates the earlier magnetic, winding and performance values required to produce a physically connected tank result.

### Optimal

Searches the configured design space, rejects infeasible candidates and returns a ranked Pareto candidate set. Review the recommended candidate together with the engineering checks; the lowest numerical score is not independent of design constraints.

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
