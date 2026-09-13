import {existsSync} from 'node:fs';
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const localPython=process.platform==='win32'?path.join(root,'tx-3d/.venv/Scripts/python.exe'):path.join(root,'tx-3d/.venv/bin/python');
const python=process.env.TX_CAD_PYTHON||localPython;
let running=false;
export function cadStatus(){return {available:existsSync(python)&&existsSync(path.join(root,'tx-3d/build.py')),busy:running,hasModel:existsSync(path.join(root,'dist/cad/assets/manifest.json'))};}
export async function buildCad(){
  if(running)throw new Error('A CAD generation is already running. Wait for it to finish.');
  if(!cadStatus().available)throw new Error('CAD generation is unavailable on this server. Use npm run build:cad locally or configure TX_CAD_PYTHON.');
  running=true;
  let workspace;
  try{
    workspace=await fs.mkdtemp(path.join(root,'tmp-cad-'));
    // Capture one complete result so concurrent C runs cannot alter this build.
    await fs.copyFile(path.join(root,'data/output.json'),path.join(workspace,'source.json'));
    return await new Promise((resolve,reject)=>{
      const child=spawn(python,[path.join(root,'tx-3d/build.py'),'--input',path.join(workspace,'source.json')],{cwd:root,windowsHide:true});
      let stdout='',stderr='';
      const timeout=setTimeout(()=>{child.kill();reject(new Error('CAD generation exceeded the five-minute limit.'));},300000);
      child.stdout.on('data',data=>{stdout=(stdout+data).slice(-30000);});
      child.stderr.on('data',data=>{stderr=(stderr+data).slice(-10000);});
      child.on('error',error=>{clearTimeout(timeout);reject(error);});
      child.on('close',code=>{clearTimeout(timeout);code===0?resolve({ok:true,output:stdout}):reject(new Error(stderr.trim()||stdout.trim()||`CAD exited ${code}`));});
    });
  }finally{
    running=false;
    // This directory is created above specifically for one snapshot and cannot be user supplied.
    if(workspace)await fs.rm(workspace,{recursive:true,force:true});
  }
}
