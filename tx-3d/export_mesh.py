"""glTF 2.0 viewer derivative tessellated exclusively from the CAD solids.

Preserves a named node for every component and reuses repeated geometry.
The STEP file remains the exact, curved-surface interchange artifact.
"""
import json
import struct
import numpy as np
from assembly import MATERIALS


def gltf_vector(mm): return [mm[0]/1000, mm[2]/1000, -mm[1]/1000]


def write_glb(parts, path, tolerance, angle):
    binary=bytearray()
    doc={'asset':{'version':'2.0','generator':'TX-C CadQuery solid tessellation'},'scene':0,
         'scenes':[{'nodes':[]}],'nodes':[],'meshes':[],'materials':[], 'accessors':[], 'bufferViews':[], 'buffers':[]}
    materials={}
    for key,value in MATERIALS.items():
        color=list(value['color']); color += [1.0] if len(color)==3 else []
        # glTF factors are linear; our CAD palette is authored in display sRGB.
        color[:3]=[c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4 for c in color[:3]]
        material={'name':key,'pbrMetallicRoughness':{'baseColorFactor':color,'metallicFactor':value['metalness'],'roughnessFactor':value['roughness']}}
        if color[3]<1: material.update(alphaMode='BLEND',doubleSided=True)
        materials[key]=len(doc['materials']); doc['materials'].append(material)
    def array(values, component_type, kind, target, bounds=False):
        while len(binary)%4: binary.append(0)
        view=len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer':0,'byteOffset':len(binary),'byteLength':values.nbytes,'target':target})
        binary.extend(values.tobytes())
        accessor={'bufferView':view,'componentType':component_type,'count':len(values),'type':kind}
        if bounds: accessor.update(min=values.min(axis=0).tolist(),max=values.max(axis=0).tolist())
        doc['accessors'].append(accessor)
        return len(doc['accessors'])-1
    geometry={}; triangles=0
    for part in parts:
        # Repeated OCP shapes can reuse vertex buffers while retaining distinct part nodes.
        shape_key=part.shape.hashCode()
        if shape_key not in geometry:
            vertices,faces=part.shape.tessellate(tolerance,angle)
            p=np.asarray([gltf_vector(v.toTuple()) for v in vertices],dtype='<f4')
            f=np.asarray(faces,dtype='<u4')
            normals=np.zeros_like(p)
            vectors=np.cross(p[f[:,1]]-p[f[:,0]],p[f[:,2]]-p[f[:,0]])
            for i in range(3): np.add.at(normals,f[:,i],vectors)
            norm=np.linalg.norm(normals,axis=1); norm[norm==0]=1
            normals/=norm[:,None]
            attrs={'POSITION':array(p,5126,'VEC3',34962,True),'NORMAL':array(normals,5126,'VEC3',34962)}
            indices=array(f.reshape(-1),5125,'SCALAR',34963)
            geometry[shape_key]=(attrs,indices,len(f))
        attrs,indices,count=geometry[shape_key]; triangles+=count
        mesh=len(doc['meshes'])
        doc['meshes'].append({'name':part.name,'primitives':[{'attributes':attrs,'indices':indices,'material':materials[part.material]}]})
        node={'name':part.name,'mesh':mesh,'translation':gltf_vector(part.position),
              'extras':{'partId':part.name,'group':part.group,'stage':part.stage,'explode':gltf_vector(part.explode)}}
        doc['scenes'][0]['nodes'].append(len(doc['nodes'])); doc['nodes'].append(node)
    while len(binary)%4: binary.append(0)
    doc['buffers']=[{'byteLength':len(binary)}]
    encoded=json.dumps(doc,separators=(',',':')).encode()
    encoded+=b' '*((-len(encoded))%4)
    with path.open('wb') as stream:
        stream.write(struct.pack('<4sII',b'glTF',2,12+8+len(encoded)+8+len(binary)))
        stream.write(struct.pack('<I4s',len(encoded),b'JSON')); stream.write(encoded)
        stream.write(struct.pack('<I4s',len(binary),b'BIN\x00')); stream.write(binary)
    return {'renderedTriangles':triangles,'uniqueGeometries':len(geometry),'sizeBytes':path.stat().st_size}
