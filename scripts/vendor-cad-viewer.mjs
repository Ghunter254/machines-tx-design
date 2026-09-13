// Download pinned viewer dependencies; CAD geometry is generated separately.
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const version='0.183.2';
const files=['build/three.module.js','build/three.core.js','examples/jsm/controls/OrbitControls.js',
  'examples/jsm/loaders/GLTFLoader.js','examples/jsm/utils/BufferGeometryUtils.js','examples/jsm/utils/SkeletonUtils.js','examples/jsm/environments/RoomEnvironment.js','LICENSE'];
for(const file of files){
  const response=await fetch(`https://cdn.jsdelivr.net/npm/three@${version}/${file}`);
  if(!response.ok) throw new Error(`${file}: ${response.status}`);
  const target=path.join(root,'dist/cad/vendor/three',file);
  await fs.mkdir(path.dirname(target),{recursive:true});
  await fs.writeFile(target,new Uint8Array(await response.arrayBuffer()));
  console.log(`three ${version}: ${file}`);
}
