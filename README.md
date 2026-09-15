# TX-C Transformer Design Studio

The project is split into two deployable surfaces: `dist/` is the static frontend for Vercel, while `backend/`, `src/`, `include/`, `data/` and `build/` form the C-backed API for Render or another container host.

Dashboard operation, run modes and troubleshooting are documented in `DASHBOARD_GUIDE.md`.
The textbook reconciliation and automated equation coverage are documented in `FORMULA_AUDIT.md`.
The parametric CadQuery solid assembly, STEP downloads, build/disassembly viewer and regeneration instructions are documented in [tx-3d/README.md](tx-3d/README.md). Open `/cad/` on the local server, or use the dashboard's **3D assembly** link.

## Run locally

```text
node scripts/build-c.mjs
node backend/server.mjs
```

Open `http://127.0.0.1:5173`. The dashboard includes nominal, explore and optimal modes, a scenario library, metric views, prerequisite-aware section runs and engineering interpretation checks.

If the Windows MSYS2 installation reports `cc1.exe` application error `0xc0000022`, run the build and backend from Ubuntu/WSL or use the included Dockerfile. The dashboard itself does not require Node packages.

The source of truth for assumptions is `data/config.txt`. It contains inputs only; calculated values are written to `data/output.txt` and `data/output.json` after every run. Scenario explanations and metric views live in `dist/scenarios.js`.

## Vercel + Render deployment

1. Push the repository to GitHub.
2. Create a Render Web Service from the repository using `render.yaml` or the root `Dockerfile`.
3. Confirm `https://YOUR-RENDER-SERVICE.onrender.com/api/health` reports `executable: true`.
4. Copy `vercel.json.example` to `vercel.json` and replace the Render URL.
5. Import the repository into Vercel and use `dist` as the output directory.
6. Deploy Vercel. The frontend keeps calling `/api/*`; the rewrite proxies those calls to Render.

The C program is compiled once during the Render Docker build. Clicking Run recalculates the compiled model; it does not recompile C code.

## Direct C usage

```text
build\txsim.exe --mode nominal
build\txsim.exe --mode explore --sections lv --set APPARENT_POWER=800 --set HV_CONNECTION=star
build\txsim.exe --mode optimize --stdout-json
```

The benchmark checks are local comparison aids, not type-test certification. Manufacturing release still requires dielectric, short-circuit, thermal and routine testing against the applicable IEC requirements.

## Optimal comparison and presentation

The core-type search follows §5.2.9–5.2.10: nested loops over K, Bm, average current density and L/(D−d). The default grid is 6 × 7 × 7 × 6 = 1,764 attempts, with configurable bounds and sample counts (maximum 5,000). One sample fixes a variable at its minimum. K is the volts-per-turn factor, distinct from the stepped-core factor k. R1/R2 belong to the shell-type reference and are not search variables here.

After a fresh Optimal run, the comparison shows 15 evenly spaced attempts with original serial numbers. All feasible attempts compete for maximum full-load efficiency at 0.85 PF, minimum active kg/kVA, minimum no-load current percentage and minimum tank volume. Each winner has a **Run this design** button that restores the search's complete input configuration, applies its four variables, enables automatic conductor sizing and recalculates all six sections. The separate geometry table and text report contain the remaining textbook columns. If no feasible row exists, the best calculated row is retained as a clearly labelled diagnostic. Exact ties choose the earliest serial. The overview efficiency is at unity PF.

The balanced choice is ranked across the complete feasible Pareto set before retaining the best 32 results. It minimizes the equal-weight Euclidean distance of normalized loss, active mass and material cost index. Feasibility uses the configurable physical-fit guardrails and textbook-style objective limits in `data/config.txt`; the relaxed defaults require physical non-overlap, non-negative axial fit, actual current density of 2.15–3.6 A/mm2, efficiency at least 98%, I0/I2 at most 1%, active mass at most 4 kg/kVA, tank volume at most 1.5 m3 and temperature rise within target +0.5 C. The stricter academic 7 mm and 15 mm checks remain visible as engineering review results. Feasibility does not mean all Kenya Power benchmarks passed. Empty-feasible searches now return the sample and explicit missing recommendations instead of discarding the report.

Use **Present comparison** for eight fullscreen slides: three five-row table pages, four priority recommendations, then the balanced choice. Arrow keys navigate; the comparison also follows Yona's last slide in the complete presentation. Exit or Finish returns to the results. The same sample and recommendations are included in JSON and text reports. Redeploy both the C backend and frontend, then run Optimal again; old saved reports do not contain these fields.

Mass accounting includes all phases: the HV/LV winding sections report one phase; tank copper totals multiply those masses by the phase count. Active mass, kg/kVA, copper cost and oil displacement use these complete totals. This corrects the earlier one-phase undercount, so mass and cost results will differ from older saved reports. The existing 1% active-material allowance remains applied.

Regression checks (after rebuilding C):

```sh
node scripts/verify-optimization.mjs
node scripts/verify-formulas.mjs
# Ubuntu/WSL: synthetic frontier, ties, infeasible and failed calculations
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude tests/optimizer-ranking.c src/optimizer.c -lm -o build/test-optimizer
./build/test-optimizer
```
