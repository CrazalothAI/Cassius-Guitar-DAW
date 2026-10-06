import catalog from './startingRigs.json';
import { allParameters, byId } from './parameters.js';
import { isGlobalParameter, setParameter } from './parameterState.js';

export const startingRigs = [...catalog.captureRigs, ...catalog.rigs].map(rig => ({...rig, kind: 'rig', starter: true, ownership: 'Factory', pack: rig.assets ? 'Capture recipes' : 'Cassian starter rigs', tags: rig.styles.join(' ')}));
export function resolveStartingRigs(assets = [], preview = false) {
  return startingRigs.map(rig => {
    const missingSounds = Object.values(rig.assets || {}).filter(ref => !assets.some(a => a.id === ref.id && !a.missing)).map(ref => ref.name);
    return {...rig, unavailable: missingSounds.length > 0, missingSounds, previewUnavailable: preview && Boolean(rig.assets)};
  });
}
export function startingParameters(id) {
  const rig = startingRigs.find(rig => rig.id === id);
  if (!rig) throw new Error('Starter rig not found.');
  const values = Object.fromEntries(allParameters.filter(p => !isGlobalParameter(p.id)).map(p => [p.id, p.initial]));
  for (const [key, value] of Object.entries({...catalog.base, ...rig.parameters})) {
    const p = byId[key];
    if (!p || isGlobalParameter(key) || !Number.isFinite(value) || value < p.min || value > p.max) throw new Error('Invalid starter rig control.');
    values[key] = value;
  }
  return values;
}
export function applyStartingPreview(id) {
  const parameters = startingParameters(id); Object.entries(parameters).forEach(([id, value]) => setParameter(id, value));
  return {...startingRigs.find(rig => rig.id === id), parameters};
}
