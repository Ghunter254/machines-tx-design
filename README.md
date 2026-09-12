# TX-C Transformer Design Studio

The project is split into two deployable surfaces: `dist/` is the static frontend for Vercel, while `backend/`, `src/`, `include/`, `data/` and `build/` form the C-backed API for Render or another container host.

Dashboard operation, run modes and troubleshooting are documented in `DASHBOARD_GUIDE.md`.
The textbook reconciliation and automated equation coverage are documented in `FORMULA_AUDIT.md`.

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
