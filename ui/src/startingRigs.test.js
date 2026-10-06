import { describe, expect, it } from 'vitest';
import catalog from './startingRigs.json';
import { startingParameters, resolveStartingRigs } from './startingRigs.js';
import { allParameters, boardTypes, byId } from './parameters.js';
import { isGlobalParameter } from './parameterState.js';
import { catalogRow, matchesCatalog } from './libraryCatalog.js';

describe('complete starter definitions', () => {
  it('covers cleans and leads with valid complete controls and mono/stereo placements', () => {
    const names = catalog.rigs.map(rig => rig.name), ids = catalog.rigs.map(rig => rig.id);
    expect(new Set(ids).size).toBe(ids.length);
    expect(names).toEqual(expect.arrayContaining(['Warm Jazz', 'Glass Chorus', 'Funk Clean', 'Natural Nylon', 'Neoclassical Lead', 'Modern Metalcore']));
    for (const rig of catalog.rigs) {
      const values = startingParameters(rig.id);
      expect(Object.keys(values)).toHaveLength(allParameters.filter(p => !isGlobalParameter(p.id)).length);
      expect(values.INPUT_GAIN).toBeUndefined(); expect(values.MASTER_VOL).toBeUndefined(); expect(values.GUITAR_MIX_FOCUS).toBeUndefined();
      expect([1,2,4]).toContain(values.AMP_SOURCE); expect(values.PEDAL_ON).toBe(0);
      for (const [id, value] of Object.entries(values)) { expect(value).toBeGreaterThanOrEqual(byId[id].min); expect(value).toBeLessThanOrEqual(byId[id].max); }
      expect(new Set(rig.board.map(block => block.type)).size).toBe(rig.board.length);
      for (const block of rig.board) { expect(boardTypes).toContain(block.type); expect(['pre','post']).toContain(block.lane); if (['overdrive','neural-pedal'].includes(block.type)) expect(block.lane).toBe('pre'); }
    }
  });
  it('rejects unknown starter IDs before any application', () => expect(() => startingParameters('missing')).toThrow('Starter rig not found'));
  it('separates exact Red-II from Red-I and uses other heads for rock', () => {
    const red2 = catalog.captureRigs.find(r => r.id === 'factory.capture-red2-tight');
    const red1 = catalog.captureRigs.find(r => r.id === 'factory.capture-red1-lead');
    expect(red2.assets.model.name).toBe('APP-5153-Ivory-Red-II'); expect(red2.assets.model.id).not.toBe(red1.assets.model.id);
    expect(catalog.captureRigs.filter(r => r.gain === 'crunch').every(r => !r.amp.includes('Blue'))).toBe(true);
    expect(catalog.captureRigs.filter(r => r.gain === 'clean').length).toBeGreaterThanOrEqual(4);
    for (const r of catalog.captureRigs) { startingParameters(r.id); expect(r.board.length).toBeLessThanOrEqual(6); for (const ref of Object.values(r.assets)) expect(ref.id).toMatch(/^(amp|cab|pedal|ambience):[a-f0-9]{64}$/); }
  });
  it('requires every exact sound and does not mark a missing variant playable', () => {
    const rig = catalog.captureRigs[0], assets = Object.values(rig.assets).map(a => ({...a, missing:false}));
    expect(resolveStartingRigs(assets).find(r => r.id === rig.id).unavailable).toBe(false);
    assets[0].missing = true;
    expect(resolveStartingRigs(assets).find(r => r.id === rig.id).missingSounds).toContain(rig.assets.model.name);
    expect(resolveStartingRigs([]).find(r => r.id === 'factory.warm-jazz').unavailable).toBe(false);
  });
});
describe('library discovery', () => {
  const filter = {ownership: 'All', favorites: false, search: '', style: '', gain: 'all', speaker: '', pack: '', rigType: ''};
  it('matches source packs and literal hints while explicit owner metadata overrides them', () => {
    const inferred = catalogRow({name: 'Clean V30', sourceName: 'Clean V30', pack: 'Local.zip', ownership: 'User'});
    expect(inferred.gain).toBe('clean'); expect(inferred.speaker).toBe('V30'); expect(inferred.inferred).toBe(true);
    expect(matchesCatalog(inferred, {...filter, search: 'local', gain: 'clean', speaker: 'V30'}, false)).toBe(true);
    const corrected = catalogRow({...inferred, gain: '', speaker: 'Jensen', styles: 'Blues, Jazz'});
    expect(corrected.gain).toBe(''); expect(corrected.styles).toEqual(['blues','jazz']);
    expect(matchesCatalog(corrected, {...filter, gain: '', style: 'jazz', speaker: 'Jensen'}, false)).toBe(true);
    expect(matchesCatalog(corrected, {...filter, gain: 'clean'}, false)).toBe(false);
  });
  it('keeps unrelated capture names unknown and separates complete rigs from control starting points', () => {
    const amp = catalogRow({name: 'Capture 42', kind: 'amp', ownership: 'User'}); expect(amp.gain).toBe(''); expect(amp.styles).toEqual([]);
    expect(matchesCatalog(catalogRow({starter: true, styles: [], ownership: 'Factory'}), {...filter, rigType: 'controls'}, false)).toBe(false);
    expect(matchesCatalog(catalogRow({preset: true, styles: [], ownership: 'Factory'}), {...filter, rigType: 'starter'}, false)).toBe(false);
  });
});
