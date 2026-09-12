import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const outDir = path.join(root, 'build'); fs.mkdirSync(outDir, {recursive:true});
const compiler = process.env.CC || (process.platform === 'win32' ? 'gcc' : 'cc');
const sources = ['src/config.c','src/frame_design.c','src/no_load_current_design.c','src/lv_windings_design.c','src/hv_windings_design.c','src/performance_design.c','src/tank_design.c','src/simulation.c','src/optimizer.c','src/report.c','src/main.c'];
const output = path.join(outDir, process.platform === 'win32' ? 'txsim.exe' : 'txsim');
const args = ['-std=c11','-Wall','-Wextra','-Wpedantic','-Iinclude',...sources,'-lm','-o',output];
const result = spawnSync(compiler,args,{cwd:root,stdio:'inherit',shell:false});
if (result.error) { console.error(result.error.message); process.exit(1); }
if ((result.status ?? 1) !== 0 || !fs.existsSync(output)) {
  console.error(`C build did not produce ${output}. If Windows blocks cc1.exe, run the same command from an MSYS2/Developer shell.`);
  process.exit(result.status ?? 1);
}
console.log(`Built ${output}`);
