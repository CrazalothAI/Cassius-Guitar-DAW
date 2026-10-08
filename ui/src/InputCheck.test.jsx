import React from 'react';
import { act, cleanup, fireEvent, render, screen } from '@testing-library/react';
import { afterEach, beforeEach, expect, it, vi } from 'vitest';
const parameters = vi.hoisted(() => ({set: vi.fn()}));
vi.mock('./juce/bridge.js', () => ({native: true}));
vi.mock('./parameterState.js', () => ({setParameter: parameters.set, useParameter: () => 0}));
import InputCheck from './components/InputCheck.jsx';
import { beginCheck, sampleCheck, finishCheck, checkUnavailable } from './inputCheck.js';
const ready = {audioProcessing: true, sampleRate: 48000, bufferSize: 256, input: .5, inputClipped: false, overruns: 0, dropouts: 0, selectedInput: 0, deviceSettingsAvailable: true, audioDevice: {driver: 'ASIO', inputDevice: 'USB', outputDevice: 'USB', monitoring: true, activeInputs: 1, activeOutputs: 2}};
beforeEach(() => { vi.useFakeTimers(); parameters.set.mockClear(); });
afterEach(() => { cleanup(); vi.useRealTimers(); });
function measure(view, base = ready) {
  fireEvent.click(screen.getByRole('button', {name: 'Check input for 8 seconds'}));
  for (let i = 0; i < 80; i++) {
    act(() => vi.advanceTimersByTime(100));
    view.rerender(<InputCheck status={{...base}}/>);
  }
}
it('samples several seconds and only changes Input trim after an explicit apply', () => {
  const view = render(<InputCheck status={ready}/>);
  measure(view);
  expect(screen.getByText(/Highest sampled raw peak: -6.0 dBFS/)).toBeTruthy();
  expect(parameters.set).not.toHaveBeenCalled();
  fireEvent.click(screen.getByRole('button', {name: 'Apply suggested input trim'}));
  expect(parameters.set.mock.calls).toEqual([['INPUT_GAIN', -6]]);
});
it('remembers early clipping even after the player stops and withholds software trim', () => {
  const view = render(<InputCheck status={ready}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Check input for 8 seconds'}));
  view.rerender(<InputCheck status={{...ready, input: 1, inputClipped: true}}/>);
  for (let i = 0; i < 80; i++) { act(() => vi.advanceTimersByTime(100)); view.rerender(<InputCheck status={{...ready, input: .1}}/>); }
  expect(screen.getByText(/Software trim cannot repair/)).toBeTruthy();
  expect(screen.queryByRole('button', {name: 'Apply suggested input trim'})).toBeNull();
  expect(parameters.set).not.toHaveBeenCalled();
});
it('does not convert noise, silence or stale telemetry into an automatic gain boost', () => {
  for (const [input, kind] of [[0, 'silent'], [.00001, 'silent'], [.001, 'weak'], [NaN, 'unavailable']]) {
    const check = beginCheck(ready, 0);
    for (let n = 1; n <= 80; n++) sampleCheck(check, {...ready, input}, n * 100);
    expect(finishCheck(check, 8000).kind).toBe(kind);
    expect(finishCheck(check, 8000).trim).toBeUndefined();
  }
  const view = render(<InputCheck status={ready}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Check input for 8 seconds'}));
  act(() => vi.advanceTimersByTime(8000));
  expect(screen.getByText(/Not enough fresh input readings/)).toBeTruthy();
  expect(parameters.set).not.toHaveBeenCalled();
});
it('cancels on a changed input and invalidates a completed suggestion after a buffer change', () => {
  const view = render(<InputCheck status={ready}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Check input for 8 seconds'}));
  view.rerender(<InputCheck status={{...ready, selectedInput: 1}}/>);
  expect(screen.getByText(/Audio settings changed. Run a new check/)).toBeTruthy();
  view.rerender(<InputCheck status={ready}/>); measure(view);
  view.rerender(<InputCheck status={{...ready, bufferSize: 512}}/>);
  expect(screen.queryByRole('button', {name: 'Apply suggested input trim'})).toBeNull();
  view.rerender(<InputCheck status={ready}/>);
  expect(screen.queryByRole('button', {name: 'Apply suggested input trim'})).toBeNull();
});
it('blocks inactive audio, preview, muted routing, recording and take playback', () => {
  expect(checkUnavailable(ready, false)).toContain('installed');
  for (const status of [{...ready, audioProcessing: false}, {...ready, audioDevice: {...ready.audioDevice, monitoring: false}}, {...ready, audioDevice: {...ready.audioDevice, activeOutputs: 0}}, {...ready, practice: {recordMode: 1}}, {...ready, review: {playing: true}}]) {
    const view = render(<InputCheck status={status}/>);
    expect(screen.getByRole('button', {name: 'Check input for 8 seconds'}).disabled).toBe(true);
    view.unmount();
  }
  const view = render(<InputCheck status={ready}/>); measure(view);
  view.rerender(<InputCheck status={{...ready, practice: {recordMode: 1}}}/>);
  expect(screen.queryByRole('button', {name: 'Apply suggested input trim'})).toBeNull();
});
it('requires a new check after overruns or device dropouts and bounds suggested trim', () => {
  for (const changed of [{overruns: 1}, {dropouts: 1}, {dropoutRecent: true}]) {
    const check = beginCheck(ready, 0);
    for (let n = 1; n <= 80; n++) sampleCheck(check, {...ready, ...changed}, n * 100);
    expect(finishCheck(check, 8000).kind).toBe('interrupted');
  }
  const low = beginCheck(ready, 0);
  for (let n = 1; n <= 80; n++) sampleCheck(low, {...ready, input: .02}, n * 100);
  expect(finishCheck(low, 8000).trim).toBe(12);
});
it('allows checking a running DAW input without inventing interface controls', () => {
  const host = {...ready, deviceSettingsAvailable: false, audioDevice: undefined, selectedInput: undefined};
  expect(checkUnavailable(host, true)).toBe('');
  const view = render(<InputCheck status={host}/>); measure(view, host);
  expect(screen.getByRole('button', {name: 'Apply suggested input trim'})).toBeTruthy();
});
it('cancels without changing settings and does not retain a timer after closing', () => {
  const view = render(<InputCheck status={ready}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Check input for 8 seconds'}));
  fireEvent.click(screen.getByRole('button', {name: 'Cancel input check'}));
  act(() => vi.advanceTimersByTime(9000));
  expect(screen.queryByRole('button', {name: 'Apply suggested input trim'})).toBeNull();
  expect(parameters.set).not.toHaveBeenCalled();
  view.unmount(); expect(vi.getTimerCount()).toBe(0);
});
it('refuses an earlier suggestion if live polling stalls before Apply', () => {
  const view = render(<InputCheck status={ready}/>); measure(view);
  act(() => vi.advanceTimersByTime(2000));
  fireEvent.click(screen.getByRole('button', {name: 'Apply suggested input trim'}));
  expect(parameters.set).not.toHaveBeenCalled();
  expect(screen.getByText(/Live input readings stopped/)).toBeTruthy();
});
