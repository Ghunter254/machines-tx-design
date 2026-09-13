"""A true CAD half-section with closed cut faces, for the viewer."""
from dataclasses import replace
from assembly import box


def section_parts(parts):
    output=[]; cache={}
    for p in parts:
        b=p.shape.BoundingBox()
        ymin=b.ymin+p.position[1]; ymax=b.ymax+p.position[1]
        if ymax<=1e-6: continue
        if ymin>=-1e-6:
            output.append(p);continue
        key=(p.shape.hashCode(),p.position[1])
        if key not in cache:
            length=max(b.xlen,b.ylen,b.zlen)*4+10000
            cutter=box(length,length,length).translate((0,-length/2-p.position[1],-length/2))
            cut=p.shape.cut(cutter)
            if not cut.isValid(): raise ValueError(f'Invalid half-section: {p.name}')
            cache[key]=cut
        shape=cache[key]
        if shape.Volume()>1e-5: output.append(replace(p,shape=shape))
    return output
