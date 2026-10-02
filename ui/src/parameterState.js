import { useEffect, useState } from 'react';
import { slider } from './juce/bridge.js';
import { byId, parameters, toFraction } from './parameters.js';
const preview = new Map();
// Host automation gestures. A drag or held key is one gesture around many value changes.
export const beginGesture = id => slider(id)?.sliderDragStarted();
export const endGesture = id => slider(id)?.sliderDragEnded();
export function setParameter(id, value, { gesture = true } = {}) {
  const state = slider(id);
  if (state) {
    if (gesture) state.sliderDragStarted();
    state.setNormalisedValue(toFraction(byId[id], value));
    if (gesture) state.sliderDragEnded();
  } else {
    preview.set(id, value);
  }
  // JUCE updates the sender's cached value without emitting its change event.
  window.dispatchEvent(new CustomEvent('cassian-parameter', {detail: {id, value}}));
}
export function useParameter(id) {
  const state = slider(id);
  const [value, setValue] = useState(preview.get(id) ?? byId[id].initial);
  useEffect(() => {
    const localSync = e => { if (e.detail.id === id) setValue(e.detail.value); };
    window.addEventListener('cassian-parameter', localSync);
    if (!state) {
      return () => window.removeEventListener('cassian-parameter', localSync);
    }
    const sync = () => setValue(state.getScaledValue());
    sync();
    const a = state.valueChangedEvent.addListener(sync);
    const b = state.propertiesChangedEvent.addListener(sync);
    return () => { window.removeEventListener('cassian-parameter', localSync); state.valueChangedEvent.removeListener(a); state.propertiesChangedEvent.removeListener(b); };
  }, [id, state]);
  return value;
}
export const useToggle = id => useParameter(id) >= .5;
export function readParameter(id) {
  const state = slider(id);
  return state?.getScaledValue?.() ?? preview.get(id) ?? byId[id].initial;
}
// Several parameters at once, re-rendering only when one of them changes.
export function useParameters(ids) {
  const key = ids.join();
  const [values, setValues] = useState(() => Object.fromEntries(ids.map(id => [id, readParameter(id)])));
  useEffect(() => {
    const list = key.split(',');
    const refresh = () => setValues(prev => {
      const next = Object.fromEntries(list.map(id => [id, readParameter(id)]));
      return list.every(id => prev[id] === next[id]) ? prev : next;
    });
    const localSync = e => { if (list.includes(e.detail.id)) refresh(); };
    window.addEventListener('cassian-parameter', localSync);
    const subscriptions = list.map(slider).filter(Boolean).map(state =>
      [state, state.valueChangedEvent.addListener(refresh), state.propertiesChangedEvent.addListener(refresh)]);
    refresh();
    return () => {
      window.removeEventListener('cassian-parameter', localSync);
      subscriptions.forEach(([state, a, b]) => { state.valueChangedEvent.removeListener(a); state.propertiesChangedEvent.removeListener(b); });
    };
  }, [key]);
  return values;
}
// A/B compares tone, like presets: input calibration and master level stay where they are.
const calibration = new Set(['INPUT_GAIN', 'MASTER_VOL']);
export function snapshotParameters() {
  return Object.fromEntries(parameters.filter(({id}) => !calibration.has(id)).map(({id}) => [id, readParameter(id)]));
}
export function restoreSnapshot(snapshot) {
  Object.entries(snapshot ?? {}).forEach(([id, value]) => setParameter(id, value));
}
