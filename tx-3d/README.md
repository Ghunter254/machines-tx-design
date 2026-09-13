# Transformer solid-CAD assembly

This is a parametric **CadQuery / OpenCascade boundary-representation model**. The STEP download contains curved surfaces and closed solids, not triangle approximations. The browser uses a lightweight triangulated derivative of those same solids, because browser GPUs render triangles. It never invents a second set of dimensions in JavaScript.

## Open the completed model

Start the existing project server with `npm start` (or `node backend/server.mjs`), then open `http://127.0.0.1:5173/cad/`. The dashboard also has a **3D assembly** link. Python is not needed just to view the supplied model.

- **Assembled**: complete transformer and external equipment.
- **Active part**: core, LV and HV winding packs, insulation, clamps and leads.
- **Exploded** and the explosion slider: separate the component groups continuously.
- **Components**: turn individual component families on or off.
- Click a solid to inspect its dimensions, material, provenance and mass basis; hide or isolate it. **Reset** restores the view.
- **Half-section**: load an actual boolean-cut CAD derivative, with closed section faces. It is not an uncapped clipping-plane trick.
- **Show CAD edges** and **Dimension guides**: inspect boundaries and key dimensions.
- Bottom timeline: build up or take apart the assembly in eleven explanatory stages. Play advances automatically; left/right keys change stage. This sequence is illustrative, not a factory assembly procedure.
- Drag to orbit, scroll to zoom, right-drag to pan. `F` fits the visible model. Camera buttons select an orientation.
- **Download STEP** opens the complete solid assembly in FreeCAD, SolidWorks, Fusion or another STEP-capable CAD tool.

## Rebuild from the C simulation

1. Run a **complete** simulation in the main dashboard. Partial results are intentionally rejected by the CAD generator.
2. Run `npm run build:cad`, or use **Regenerate from C result** in the local viewer.
3. Refresh the CAD page after a command-line build. The on-page button refreshes automatically on success.

The generated model is a saved snapshot, not a live mesh that changes while inputs are being edited. The page compares it with the latest saved API result and warns when they differ. Regeneration does not recompile or change the C simulation.

An isolated Python environment has been prepared at `tx-3d/.venv` on this workstation. To recreate it with Python 3.12:

```powershell
py -3.12 -m venv tx-3d/.venv
tx-3d/.venv/Scripts/python.exe -m pip install -r tx-3d/requirements.txt
node scripts/build-cad.mjs
tx-3d/.venv/Scripts/python.exe tx-3d/test_design.py
```

On Linux, use `python3 -m venv tx-3d/.venv` and `tx-3d/.venv/bin/python` instead. Do not share a Windows virtual environment with WSL.

Alternatively, create the supplied Conda environment:

```text
conda env create -f tx-3d/environment.yml
conda run -n tx-cad python tx-3d/build.py
conda run -n tx-cad python tx-3d/test_design.py
```

For the Node worker to use Conda, set `TX_CAD_PYTHON` to the absolute path of that environment's Python executable, then restart the backend. No system-wide Python packages are required.

Other commands:

```text
node scripts/build-cad.mjs --input data/runs/YOUR-RUN/output.json
node scripts/build-cad.mjs --preview
node scripts/vendor-cad-viewer.mjs
```

The last command refreshes the pinned, locally served Three.js viewer dependencies; it is not necessary for normal use. `--preview` additionally creates two offscreen CAD-rendered PNGs.

CasADi is pinned to 3.6.7: the newer wheel installed initially caused an access violation when Python shut down after importing CadQuery on this Windows machine. The pinned environment completes cleanly. An analogous upstream Windows shutdown failure is tracked in [CadQuery issue 1911](https://github.com/CadQuery/cadquery/issues/1911). Do not remove the pin without rerunning the import, build and regression checks.

## Source files and output

| File | Purpose |
| --- | --- |
| `spec.py` | C-result validation and metre-to-millimetre conversion |
| `assembly.py` | Parametric solid parts, materials, placements and explosion vectors |
| `cad_assumptions.json` | Explicit construction assumptions absent from the C model |
| `build.py` | Reconciliation, exact STEP export and generated metadata |
| `section.py` | Closed half-section solids |
| `export_mesh.py` | GLB derivative with named components and shared geometry buffers |
| `test_design.py` | Solid validity, clearances, parameter changes, STEP round trip and GLB integrity tests |
| `../dist/cad/` | Standalone browser viewer and supplied generated assets |

`dist/cad/assets/transformer.step` is the exact interchange model. `manifest.json` includes every component's source, dimensions, stage, material and volume. `source-result.json` records the unmodified C result. `cad-report.txt` records the dimensional/mass reconciliation and known limitations. These outputs are generated only after validation; a failed generation leaves the previous export set in place.

## Construction and honesty about detail

The model includes three stepped limbs, both yokes, concentric LV radial/axial packs, individual HV discs, oil passages, formers and end insulation, clamps, a hollow tank, hollow cooling tubes with tank ports, a removable gasketed cover, bushings, leads, a conservator, breather, drain, lifting eyes, base channels and an oil-level dial.

Electrical dimensions come from the C output, not a manufacturer's unrelated 630 kVA outline drawing. Hardware proportions and placement are general-arrangement assumptions. Star windings have a displayed neutral bushing; delta windings have three bushings. Internal vector-group jumpers and neutral routing are not yet a fabrication wiring design.

There are deliberately no thousands of individual laminations, copper strand helices, screw threads, weld beads, gasket compression details or simulated oil flow. Winding parts are composite **copper/insulation pack envelopes**, not solid copper rings for mass calculations. Curves, shells, bores, thicknesses, component identity and physical section faces remain real CAD geometry.

Important reconciliation findings:

- The C LV/HV copper masses are **per phase**. The C tank weight sum adds each once. The CAD manifest allocates copper to all three phases and does not pretend that the C aggregate is the complete assembly mass. This task does not silently modify the electrical simulation.
- The C iron mass uses gross area and steel density; the CAD packets match that same gross-area convention. This is not a resolved lamination packing/mass model.
- The supplied run has **11.2 mm adjacent-HV clearance against the 15 mm comparison target**. It fits geometrically but remains a warning.
- Tank dimensions are interpreted as the clear internal cavity. Wall, flange, fittings and cooling tubes add to the exterior envelope.
- The optional oil domain is only a visualization aid. It overlaps the active part, is hidden by default, and is excluded from STEP and mass totals.
- The dial pointer is illustrative, not sensor data. Local hardware intersections are not a completed manufacturing interference or stress assessment.

This is a detailed student/engineering general-arrangement CAD assembly, **not a certified fabrication release**. Dielectric, thermal, pressure, weld, lifting and short-circuit designs still require engineering verification and selected rated components.

## References

External equipment arrangement was informed by the manufacturer's [Schneider Electric Minera 630 kVA product](https://www.se.com/in/en/product/MIN063011559700000/minera-ground-mounted-oil-immersed-transformer-630-kva-11kvoccbcbconvl2/) and [Minera catalogue](https://www.se.com/in/en/download/document/NRJCAT21025EN/), with [Hitachi Energy's medium distribution transformer overview](https://www.hitachienergy.com/in/en/products-and-solutions/transformers/distribution-transformers/liquid-filled-distribution-transformers/medium-distribution-transformers-316-2-499-kva) as construction context. These are reference arrangements, not claims that this model reproduces a specific manufacturer's product.

The export workflow follows the [CadQuery import/export documentation](https://cadquery.readthedocs.io/en/stable/importexport.html). Browser dependencies are Three.js 0.183.2, vendored with their MIT licence in `dist/cad/vendor/three/LICENSE`.

## Vercel and Render

The complete viewer, GLBs and STEP are ordinary static files under `dist/cad`. Your existing Vercel static deployment can serve them without Python or Conda. The existing Render C backend remains lightweight: no CAD dependencies are added to its Docker image.

To publish changed geometry, generate it locally and include the updated `dist/cad/assets` in your own commit/push. A Vercel `/api` proxy does not turn local Python into a cloud CAD generator. On a server without CadQuery, regeneration is disabled and the snapshot remains fully viewable. A dedicated Python worker would be needed for cloud-side rebuilding. Nothing has been deployed by this task.
