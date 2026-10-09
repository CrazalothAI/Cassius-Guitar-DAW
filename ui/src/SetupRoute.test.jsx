import React from 'react';
import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen } from '@testing-library/react';
import SetupRoute, {routeGuidance} from './components/SetupRoute.jsx';
const ready = {deviceSettingsAvailable: true, selectedInput: 0, inputChannels: ['Guitar'], audioProcessing: true, audioDevice: {inputDevice: 'USB interface', outputDevice: 'USB interface', monitoring: true}};
afterEach(cleanup);
it('distinguishes unavailable route state from host routing, muted monitoring, and stopped callbacks', () => {
  expect(routeGuidance({},true).kind).toBe('unknown');
  expect(routeGuidance({deviceSettingsAvailable:true},true).kind).toBe('unknown');
  expect(routeGuidance({deviceSettingsAvailable:false},true).kind).toBe('host');
  expect(routeGuidance({...ready,audioDevice:{...ready.audioDevice,monitoring:false}},true).kind).toBe('muted');
  expect(routeGuidance({...ready,audioProcessing:false},true).kind).toBe('inactive');
  expect(routeGuidance({...ready,audioProcessing:undefined,audioDevice:{...ready.audioDevice,monitoring:undefined}},true).detail).toContain('Monitoring not reported · Callback activity not reported');
});
it('shows missing or stale selected input and measured clipping/dropouts without declaring sound heard', () => {
  expect(routeGuidance({...ready,selectedInput:-1},true).title).toContain('Choose your guitar input');
  expect(routeGuidance({...ready,selectedInput:4},true).title).toContain('Choose your guitar input');
  expect(routeGuidance({...ready,inputClipped:true},true).kind).toBe('clipping');
  expect(routeGuidance({...ready,dropoutRecent:true},true).kind).toBe('dropout');
  expect(routeGuidance({...ready,selectedInput:undefined},true).title).toContain('Input not reported');
  expect(routeGuidance(ready,true).detail).toContain('Play a phrase');
});
it('offers explicit navigation without performing hardware or transport operations', () => {
  const navigate = vi.fn(); render(<SetupRoute status={{...ready,practice:{recordMode:1}}} native onNavigate={navigate}/>);
  expect(navigate).not.toHaveBeenCalled();
  fireEvent.click(screen.getByRole('button',{name:'Continue recording in Practice'}));
  fireEvent.click(screen.getByRole('button',{name:'Review & export in Takes'}));
  expect(navigate.mock.calls).toEqual([['Practice'],['Takes']]);
});
it('keeps preview guidance honest even if simulated routing fields are provided', () => {
  render(<SetupRoute status={ready} native={false}/>);
  expect(screen.getByText(/This browser previews the editor/)).toBeTruthy();
  expect(screen.queryByRole('button')).toBeNull();
});
