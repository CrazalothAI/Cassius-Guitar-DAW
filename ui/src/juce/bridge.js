import * as Juce from './index.js';
export const native = window.__JUCE__?.initialisationData?.__juce__functions?.includes('getStatus') === true;
export const slider = id => native ? Juce.getSliderState(id) : null;
export const invoke = (name, ...args) => native ? Juce.getNativeFunction(name)(...args) : Promise.resolve(null);
