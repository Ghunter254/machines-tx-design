import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {RoomEnvironment} from 'three/addons/environments/RoomEnvironment.js';

const $=id=>document.getElementById(id);
const groupNames={core:'Magnetic core',lv:'LV winding packs',hv:'HV disc windings',insulation:'Insulation & spacers',structure:'Clamps & supports',leads:'Connection leads',tank:'Fabricated tank',cooling:'Cooling tubes',cover:'Cover & hardware',terminals:'Bushings & terminals',accessories:'Conservator & fittings',fluid:'Oil domain'};
const groupColors={core:'#778591',lv:'#bf733f',hv:'#ad592e',insulation:'#bc9357',structure:'#8b9ba6',leads:'#de9c66',tank:'#55798e',cooling:'#55798e',cover:'#55798e',terminals:'#825842',accessories:'#6d8799',fluid:'#e5b555'};
const cadToScene=v=>new THREE.Vector3(v[0]/1000,v[2]/1000,-v[1]/1000);
const fmt=(v,d=2)=>Number(v).toLocaleString(undefined,{maximumFractionDigits:d});
const escape=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const canvas=$('scene');
let renderer;
try{renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:true,powerPreference:'high-performance'});}
catch(error){$('loadState').innerHTML='<strong>3D graphics unavailable</strong><p>Enable hardware acceleration, or open the STEP file in your CAD application.</p>';throw error;}
renderer.setPixelRatio(Math.min(devicePixelRatio,2));
renderer.outputColorSpace=THREE.SRGBColorSpace;
renderer.toneMapping=THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure=1.0;
renderer.shadowMap.enabled=true;
renderer.shadowMap.type=THREE.PCFSoftShadowMap;
renderer.localClippingEnabled=true;
const scene=new THREE.Scene();
const camera=new THREE.PerspectiveCamera(34,1,.01,100);
const controls=new OrbitControls(camera,canvas);
controls.enableDamping=true; controls.dampingFactor=.09; controls.minDistance=.25; controls.maxDistance=16;
const pmrem=new THREE.PMREMGenerator(renderer);
const environment=new RoomEnvironment();
scene.environment=pmrem.fromScene(environment,.04).texture;
environment.dispose(); pmrem.dispose();
scene.add(new THREE.HemisphereLight(0xe2f0ff,0x586572,.55));
const key=new THREE.DirectionalLight(0xfff5e8,2.3);key.position.set(2,6,4);key.castShadow=true;
key.shadow.mapSize.set(2048,2048);Object.assign(key.shadow.camera,{left:-3,right:3,top:4,bottom:-3,near:.1,far:15});key.shadow.bias=-.0003;scene.add(key);
const fill=new THREE.DirectionalLight(0xa9c9f3,.8);fill.position.set(-4,3,-2);scene.add(fill);
const floor=new THREE.Mesh(new THREE.PlaneGeometry(80,80),new THREE.ShadowMaterial({color:0x000000,opacity:.25}));floor.rotation.x=-Math.PI/2;floor.position.y=-.008;floor.receiveShadow=true;scene.add(floor);
const grid=new THREE.GridHelper(12,48,0x607384,0x526475);grid.material.transparent=true;grid.material.opacity=.08;grid.position.y=-.007;scene.add(grid);
let manifest,source,assembly,selected=null,playTimer=null,currentView='assembled';
const parts=new Map(),hiddenParts=new Set(),enabledGroups=new Set();
const sectionGeometry=new Map();
let sectionLoaded=false;
const guides=new THREE.Group();scene.add(guides);
const labels=[];
let explosion=0,targetExplosion=0,stage=11;

function notice(text){$('notice').textContent=text;$('notice').hidden=false;clearTimeout(notice.timer);notice.timer=setTimeout(()=>$('notice').hidden=true,7000);}
function closeInspector(){if(selected){parts.get(selected)?.meshes.forEach(m=>m.material.emissive.setHex(0));}selected=null;$('inspection').hidden=true;}
function choosePart(id){
  closeInspector();const part=parts.get(id);if(!part)return;selected=id;
  part.meshes.forEach(m=>{m.material.emissive.setHex(0x527fa8);m.material.emissiveIntensity=.24;});
  const p=part.info;$('partGroup').textContent=groupNames[p.group];$('partName').textContent=p.label;
  let properties=[['Material',p.material.replaceAll('_',' ')],['Solid bodies',p.solidCount],['CAD volume',`${fmt(p.volumeMm3/1e6,3)} L`]];
  if(p.analyticalCopperMassKg!=null)properties.push(['C copper allocation',`${fmt(p.analyticalCopperMassKg,3)} kg`]);
  else if(p.cadMaterialMassKg!=null)properties.push(['Geometric mass',`${fmt(p.cadMaterialMassKg,3)} kg`]);
  Object.entries(p.dimensions).slice(0,5).forEach(([key,value])=>properties.push([key.replace(/Mm$/,' (mm)').replace(/([A-Z])/g,' $1'),fmt(value,2)]));
  $('partProperties').innerHTML=properties.map(([k,v])=>`<div><dt>${escape(k)}</dt><dd>${escape(v)}</dd></div>`).join('');
  $('partSource').textContent=p.provenance;$('inspection').hidden=false;
}
function updateVisibility(){
  if(!manifest)return;
  for(const part of parts.values()){
    part.node.visible=enabledGroups.has(part.info.group)&&!hiddenParts.has(part.info.id)&&part.info.stage<=stage&&(!$('cutaway').checked||sectionGeometry.has(part.info.id));
    if(currentView==='active'&&part.info.id.startsWith('base_'))part.node.visible=false;
    part.edges.forEach(edge=>edge.visible=$('edges').checked);
  }
  if(selected&&!parts.get(selected)?.node.visible)closeInspector();
  document.querySelectorAll('[data-group]').forEach(input=>input.checked=enabledGroups.has(input.dataset.group));
  guides.visible=$('dimensions').checked;
  $('stage').value=stage;$('stageCount').textContent=`${stage} / 11`;
  $('stageName').textContent=stage===11?'Complete transformer':manifest.stages[stage-1];
}
function stopPlaying(){clearInterval(playTimer);playTimer=null;$('play').textContent='▶';$('play').setAttribute('aria-label','Play assembly sequence');}
function setView(view){
  if(!assembly)return;
  stopPlaying();closeInspector();currentView=view;hiddenParts.clear();stage=11;
  enabledGroups.clear();Object.keys(groupNames).forEach(group=>{if(group!=='fluid')enabledGroups.add(group);});
  if(view==='active')['tank','cover','cooling','accessories','terminals'].forEach(group=>enabledGroups.delete(group));
  targetExplosion=view==='exploded'?.55:0; $('explode').value=targetExplosion*100;$('explodeValue').textContent=`${Math.round(targetExplosion*100)}%`;
  document.querySelectorAll('[data-view]').forEach(b=>b.classList.toggle('active',b.dataset.view===view));
  $('viewTitle').textContent={assembled:'Complete assembly',active:'Core, windings & insulation',exploded:'Exploded assembly'}[view];
  updateVisibility();fitCamera('iso',view==='exploded'?1.5:1);
}
function modelBounds(){
  const bounds=new THREE.Box3();
  for(const part of parts.values())if(part.node.visible&&part.info.group!=='fluid')bounds.expandByObject(part.node);
  return bounds;
}
function fitCamera(orientation='iso',extra=1){
  if(!assembly)return;
  const bounds=modelBounds();if(bounds.isEmpty())return;
  const center=bounds.getCenter(new THREE.Vector3());const size=bounds.getSize(new THREE.Vector3());
  const fov=THREE.MathUtils.degToRad(camera.fov);
  let distance=Math.max(size.y,size.x/camera.aspect,size.z)*.5/Math.tan(fov/2)*1.30*extra;
  const dirs={iso:[1,.65,1.55],front:[0,.06,1],right:[1,.06,0],top:[0,1,.0001]};
  const dir=new THREE.Vector3(...dirs[orientation]).normalize();
  controls.target.copy(center);camera.position.copy(center).addScaledVector(dir,distance);camera.lookAt(center);controls.update();
}
function makeDimensions(){
  const d=manifest.dimensions, [l,w,h]=d.tankInternalMm;
  const bottom=manifest.cadAssumptions.baseHeightMm;
  const data=[{a:[-l/2,-w/2-140,bottom],b:[l/2,-w/2-140,bottom],text:`Tank cavity ${fmt(l,1)} mm`},
    {a:[l/2+170,-w/2,bottom],b:[l/2+170,-w/2,bottom+h],text:`${fmt(h,1)} mm`},
    {a:[-d.limbPitchMm,0,bottom+h+160],b:[0,0,bottom+h+160],text:`Limb pitch ${fmt(d.limbPitchMm,1)} mm`}];
  data.forEach(({a,b,text})=>{
    const aa=cadToScene(a),bb=cadToScene(b);
    const line=new THREE.Line(new THREE.BufferGeometry().setFromPoints([aa,bb]),new THREE.LineBasicMaterial({color:0xa9cfea,depthTest:false,transparent:true,opacity:.8}));line.renderOrder=5;guides.add(line);
    for(const point of [aa,bb]){const mark=new THREE.Mesh(new THREE.SphereGeometry(.008,8,8),new THREE.MeshBasicMaterial({color:0xc5e4f8,depthTest:false}));mark.position.copy(point);mark.renderOrder=5;guides.add(mark);}
    const label=document.createElement('span');label.className='dimension-label';label.textContent=text;$('dimensionOverlay').append(label);
    labels.push({element:label,position:aa.clone().lerp(bb,.5)});
  });guides.visible=false;
}
async function loadModel(){
  const version=Date.now();
  [manifest,source]=await Promise.all(['manifest.json','source-result.json'].map(async file=>{const r=await fetch(`./assets/${file}?v=${version}`);if(!r.ok)throw Error(`Missing ${file}; generate the CAD assembly first.`);return r.json();}));
  const gltf=await new GLTFLoader().loadAsync(`./assets/transformer.glb?v=${encodeURIComponent(manifest.generatedAt)}`);
  assembly=gltf.scene;scene.add(assembly);
  const infos=new Map(manifest.parts.map(p=>[p.id,p]));
  assembly.traverse(node=>{
    const id=node.userData.partId;if(!id||!infos.has(id))return;
    const info=infos.get(id),meshes=[],edges=[];
    node.traverse(child=>{if(!child.isMesh)return;child.material=child.material.clone();child.castShadow=info.group!=='fluid';child.receiveShadow=true;child.material.envMapIntensity=.65;
      child.material.clippingPlanes=[];child.material.side=THREE.DoubleSide;meshes.push(child);child.userData.ownerId=id;
    });
    for(const mesh of meshes){const edge=new THREE.LineSegments(new THREE.EdgesGeometry(mesh.geometry,28),new THREE.LineBasicMaterial({color:0x14212a,transparent:true,opacity:.22}));edge.visible=false;mesh.add(edge);edges.push(edge);}
    parts.set(id,{info,node,meshes,edges,fullGeometry:meshes.map(m=>m.geometry),base:node.position.clone(),explode:cadToScene(info.explodeMm)});
  });
  if(parts.size!==manifest.partCount)throw Error(`CAD node mapping incomplete (${parts.size}/${manifest.partCount})`);
  $('modelName').textContent=manifest.name;$('modelMeta').textContent=`${manifest.partCount} parts · ${manifest.solidCount} solid bodies`;
  $('geometryId').textContent=`GEOMETRY ${manifest.fingerprint.toUpperCase()}`;
  $('meshStats').textContent=`${fmt(manifest.mesh.sizeBytes/1e6,1)} MB · CAD-derived surfaces`;
  $('groupList').innerHTML=Object.entries(groupNames).filter(([id])=>manifest.groups[id]).map(([id,label])=>`<label class="group-row"><input data-group="${id}" type="checkbox" ${id!=='fluid'?'checked':''}><span><i class="swatch" style="background:${groupColors[id]}"></i>${label}</span><small>${manifest.groups[id]}</small></label>`).join('');
  $('groupList').addEventListener('change',e=>{const group=e.target.dataset.group;if(!group)return;e.target.checked?enabledGroups.add(group):enabledGroups.delete(group);updateVisibility();});
  makeDimensions();setView('assembled');$('loadState').hidden=true;
}
function resize(){const r=canvas.parentElement.getBoundingClientRect();renderer.setSize(r.width,r.height,false);camera.aspect=r.width/r.height;camera.updateProjectionMatrix();}
new ResizeObserver(resize).observe(canvas.parentElement);resize();
renderer.setAnimationLoop(()=>{
  explosion+=(targetExplosion-explosion)*.12;
  if(Math.abs(targetExplosion-explosion)<.0001)explosion=targetExplosion;
  for(const p of parts.values())p.node.position.copy(p.base).addScaledVector(p.explode,explosion);
  controls.update();renderer.render(scene,camera);
  for(const label of labels){const v=label.position.clone().project(camera);label.element.hidden=!guides.visible||v.z>1;label.element.style.left=`${(v.x*.5+.5)*canvas.clientWidth}px`;label.element.style.top=`${(-v.y*.5+.5)*canvas.clientHeight}px`;}
});
const raycaster=new THREE.Raycaster();let down=null;
canvas.addEventListener('pointerdown',e=>down={x:e.clientX,y:e.clientY});
canvas.addEventListener('pointerup',e=>{
  if(!down||Math.hypot(e.clientX-down.x,e.clientY-down.y)>5)return;
  const rect=canvas.getBoundingClientRect();raycaster.setFromCamera(new THREE.Vector2((e.clientX-rect.left)/rect.width*2-1,-(e.clientY-rect.top)/rect.height*2+1),camera);
  const targets=[...parts.values()].filter(p=>p.node.visible&&p.info.group!=='fluid').flatMap(p=>p.meshes);
  const hit=raycaster.intersectObjects(targets,false)[0];
  hit?choosePart(hit.object.userData.ownerId):closeInspector();
});
$('closeInspection').onclick=closeInspector;
$('hidePart').onclick=()=>{if(selected)hiddenParts.add(selected);updateVisibility();};
$('isolate').onclick=()=>{if(!selected)return;for(const id of parts.keys())if(id!==selected)hiddenParts.add(id);updateVisibility();fitCamera();};
$('restore').onclick=()=>setView(currentView);
document.querySelectorAll('[data-view]').forEach(button=>button.onclick=()=>setView(button.dataset.view));
document.querySelectorAll('[data-camera]').forEach(button=>button.onclick=()=>fitCamera(button.dataset.camera));
$('fit').onclick=()=>fitCamera();
$('explode').oninput=e=>{stopPlaying();targetExplosion=Number(e.target.value)/100;$('explodeValue').textContent=`${e.target.value}%`;};
$('edges').onchange=updateVisibility;
$('dimensions').onchange=updateVisibility;
$('cutaway').onchange=async()=>{
  if(!manifest)return;
  if($('cutaway').checked&&!sectionLoaded){
    $('cutaway').disabled=true;
    try{
      const result=await new GLTFLoader().loadAsync(`./assets/transformer-section.glb?v=${encodeURIComponent(manifest.generatedAt)}`);
      result.scene.traverse(node=>{if(node.userData.partId){const meshes=[];node.traverse(child=>{if(child.isMesh)meshes.push(child.geometry);});sectionGeometry.set(node.userData.partId,meshes);}});
      sectionLoaded=true;
    }catch(error){$('cutaway').checked=false;notice(`Half-section unavailable: ${error.message}`);}
    finally{$('cutaway').disabled=false;}
  }
  for(const p of parts.values())p.meshes.forEach((m,i)=>{
    m.geometry=($('cutaway').checked?sectionGeometry.get(p.info.id)?.[i]:p.fullGeometry[i])||p.fullGeometry[i];
    p.edges[i].geometry.dispose();p.edges[i].geometry=new THREE.EdgesGeometry(m.geometry,28);
  });
  updateVisibility();
};
$('stage').oninput=e=>{stopPlaying();stage=Number(e.target.value);updateVisibility();};
$('previousStage').onclick=()=>{stopPlaying();stage=Math.max(1,stage-1);updateVisibility();};
$('nextStage').onclick=()=>{stopPlaying();stage=Math.min(11,stage+1);updateVisibility();};
$('play').onclick=()=>{
  if(!assembly)return;
  if(playTimer){stopPlaying();return;}setView('assembled');stage=1;updateVisibility();$('play').textContent='Ⅱ';$('play').setAttribute('aria-label','Pause assembly sequence');
  playTimer=setInterval(()=>{stage+=1;updateVisibility();if(stage>=11)stopPlaying();},1800);
};
$('fullscreen').onclick=()=>document.fullscreenElement?document.exitFullscreen():document.documentElement.requestFullscreen?.();
$('mobileControls').onclick=()=>{$('mobileControls').setAttribute('aria-expanded',document.querySelector('.navigator').classList.toggle('mobile-open'));};
window.addEventListener('keydown',e=>{if(['INPUT','TEXTAREA'].includes(document.activeElement?.tagName))return;if(e.key==='Escape')closeInspector();if(e.key.toLowerCase()==='f')fitCamera();if(e.key==='ArrowRight')$('nextStage').click();if(e.key==='ArrowLeft')$('previousStage').click();});
async function cadStatus(){
  try{const response=await fetch('/api/cad/status');if(!response.ok)throw Error();const status=await response.json();
    $('rebuild').disabled=!status.available||status.busy;
    $('buildHint').textContent=status.available?'Regenerates solids from the latest complete local C run.':'Viewing a generated CAD snapshot. Regenerate locally with npm run build:cad.';
  }catch{$('rebuild').disabled=true;$('buildHint').textContent='CAD snapshot ready to inspect. Regenerate locally with npm run build:cad.';}
}
async function checkSnapshot(){
  const stable=v=>v&&typeof v==='object'?Array.isArray(v)?v.map(stable):Object.fromEntries(Object.keys(v).sort().map(k=>[k,stable(v[k])])):v;
  const relevant=v=>JSON.stringify(stable(Object.fromEntries(['input','assumptions','sections'].map(k=>[k,v[k]]))));
  try{
    const r=await fetch('/api/output.json');if(!r.ok)return;
    const latest=await r.json();if(!latest.sections)return;
    $('snapshotState').textContent=relevant(source)===relevant(latest)?'Matches the latest saved C result.':'CAD snapshot differs from the latest C run. Regenerate a complete result to update the model.';
  }catch{/* Static hosting remains fully usable without the C API. */}
}
$('rebuild').onclick=async()=>{
  $('rebuild').disabled=true;$('buildHint').textContent='OpenCascade is rebuilding the solid assembly. This may take a minute.';
  try{const r=await fetch('/api/cad/build',{method:'POST'});const result=await r.json();if(!r.ok)throw Error(result.error||'CAD regeneration failed');location.reload();}
  catch(error){notice(error.message);await cadStatus();}
};
try{await loadModel();await cadStatus();await checkSnapshot();}catch(error){$('loadState').innerHTML=`<strong>Unable to load assembly</strong><p>${escape(error.message)}</p>`;console.error(error);}
