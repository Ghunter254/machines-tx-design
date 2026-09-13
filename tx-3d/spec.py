"""Validate the C result and convert its geometry to millimetres once."""
from dataclasses import dataclass
from hashlib import sha256
import json
from math import isfinite
from pathlib import Path


def fingerprint(data):
    payload = {key: data[key] for key in ('input', 'assumptions', 'sections')}
    return sha256(json.dumps(payload, sort_keys=True, separators=(',', ':')).encode()).hexdigest()[:16]


def positive(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not isfinite(value) or value <= 0:
        raise ValueError(f'{name} must be a finite positive number')
    return float(value)


@dataclass
class Specification:
    source: dict
    cad: dict

    @classmethod
    def load(cls, source_path, assumptions_path):
        source = json.loads(Path(source_path).read_text(encoding='utf-8-sig'))
        cad = json.loads(Path(assumptions_path).read_text(encoding='utf-8-sig'))
        spec = cls(source, cad)
        spec.validate()
        return spec

    @property
    def frame(self): return self.source['sections']['frame']
    @property
    def lv(self): return self.source['sections']['lv']
    @property
    def hv(self): return self.source['sections']['hv']
    @property
    def tank(self): return self.source['sections']['tank']
    @property
    def assumptions(self): return self.source['assumptions']
    @property
    def d(self): return self.frame['coreDiameterM'] * 1000
    @property
    def pitch(self): return self.frame['centreDistanceM'] * 1000
    @property
    def window_height(self): return self.frame['coreLengthM'] * 1000
    @property
    def yoke_height(self): return self.frame['yokeHeightM'] * 1000
    @property
    def floor(self): return self.cad['baseHeightMm']
    @property
    def core_bottom(self): return self.floor + self.cad['activePartBottomClearanceMm']
    @property
    def winding_mid(self): return self.core_bottom + self.yoke_height + self.window_height / 2
    @property
    def tank_top(self): return self.floor + self.tank['heightM'] * 1000
    @property
    def cover_top(self): return self.tank_top + self.cad['flangeThicknessMm'] + self.cad['gasketThicknessMm'] + self.cad['coverThicknessMm']

    def validate(self):
        if self.source.get('input', {}).get('phases') != 3:
            raise ValueError('This assembly generator requires a three-phase core-type design.')
        if self.source['input'].get('cooling')!='ONAN':
            raise ValueError('This construction models ONAN cooling, not forced-air or forced-oil equipment.')
        for side in ('hv','lv'):
            if str(self.source['input'].get(side+'Connection','')).lower() not in ('star','delta'):
                raise ValueError('Bushing arrangement requires a star or delta winding connection')
        required = {'frame', 'no-load', 'lv', 'hv', 'performance', 'tank'}
        if not required.issubset(self.source.get('meta', {}).get('completedSections', [])):
            raise ValueError('Run the complete C calculation before generating the CAD assembly.')
        for family in ('frame', 'lv', 'hv', 'tank'):
            if family not in self.source.get('sections', {}):
                raise ValueError(f'Missing {family} results')
        for key in ('coreDiameterM', 'coreLengthM', 'grossCoreAreaM2', 'yokeHeightM', 'yokeWidthM', 'yokeLengthM', 'centreDistanceM'):
            positive(self.frame[key], f'frame.{key}')
        for family in ('lv', 'hv'):
            winding = getattr(self, family)
            for key in ('innerDiameterMm', 'outerDiameterMm', 'activeHeightMm', 'occupiedHeightMm', 'copperVolumeM3', 'copperMassKg'):
                positive(winding[key], f'{family}.{key}')
            if winding['outerDiameterMm'] <= winding['innerDiameterMm']:
                raise ValueError(f'{family} diameters are inverted')
            if winding['occupiedHeightMm'] > self.window_height:
                raise ValueError(f'{family} winding exceeds the available core window. Resolve the C geometry first.')
            if winding['activeHeightMm'] >= winding['occupiedHeightMm']:
                raise ValueError(f'{family} active winding must leave positive end-insulation space')
        for key in ('lengthM', 'widthM', 'heightM', 'tubeDiameterM', 'tubeHeightM'):
            positive(self.tank[key], f'tank.{key}')
        for key, value in self.cad.items():
            if isinstance(value, (int, float)) and key != 'schemaVersion': positive(value, f'cad.{key}')
        nt = self.tank['coolingTubes']
        if not isinstance(nt, int) or nt < 0 or nt > 300:
            raise ValueError('Cooling-tube count is outside the supported 0–300 assembly range')
        if self.hv['coils'] != int(self.hv['coils']) or not 3 <= self.hv['coils'] <= 80:
            raise ValueError('HV coil count must be an integer from 3 to 80')
        if self.pitch <= self.hv['outerDiameterMm']:
            raise ValueError('Adjacent HV winding solids would overlap. Resolve the C winding dimensions first.')
        if self.lv['innerDiameterMm']<=self.d or self.hv['innerDiameterMm']<=self.lv['outerDiameterMm']:
            raise ValueError('Core and concentric winding envelopes overlap')
        if self.tank['widthM']*1000<=self.hv['outerDiameterMm'] or self.tank['lengthM']*1000<=2*self.pitch+self.hv['outerDiameterMm']:
            raise ValueError('Active-part footprint does not fit the tank cavity')
        if self.core_bottom+2*self.yoke_height+self.window_height>=self.tank_top:
            raise ValueError('Core assembly does not fit below the tank cover')
        for key in ('turnsAxially','turnsRadially'):
            value=self.lv[key]
            if value!=int(value) or not 1<=value<=100:
                raise ValueError(f'LV {key} must be an integer from 1 to 100')
        if self.lv['turnsAxially']*self.lv['turnsRadially']>1000:
            raise ValueError('LV pack count exceeds the lightweight assembly limit')
        if self.tank['tubeHeightM']*1000+self.tank['tubeDiameterM']*1000>=self.tank['heightM']*1000:
            raise ValueError('Cooling-tube outside envelope exceeds the tank height')
        if self.cad['conservatorWallMm']*2>=self.cad['conservatorDiameterMm']:
            raise ValueError('Conservator wall consumes the internal cavity')
        if self.cad['tubeWallMm'] * 2 >= self.tank['tubeDiameterM'] * 1000:
            raise ValueError('Cooling-tube wall consumes the bore')
        fractions = self.cad['corePacketHalfDepthFractions']
        if len(fractions)<2 or fractions[0] != 0 or fractions[-1] >= 1 or any(not isfinite(v) for v in fractions) or any(a >= b for a, b in zip(fractions, fractions[1:])):
            raise ValueError('Core packet depth fractions must increase from zero to less than one')
