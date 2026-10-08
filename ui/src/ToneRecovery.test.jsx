import {afterEach, beforeEach, expect, it, vi} from 'vitest';
import {cleanup, fireEvent, render, screen, waitFor} from '@testing-library/react';
const bridge = vi.hoisted(() => ({invoke:vi.fn(),native:true,status:{}}));
vi.mock('./juce/bridge.js', () => ({get native() {return bridge.native;},invoke:bridge.invoke}));
import ToneRecovery from './components/ToneRecovery.jsx';
beforeEach(() => {bridge.native = true; bridge.status = {available:true,automatic:true,busy:false,snapshots:[{id:'tone-first',name:'Warm clean',created:'2026-10-08T12:00:00Z'}]}; bridge.invoke.mockReset().mockImplementation(async name => name === 'getToneRecovery' ? bridge.status : '');});
afterEach(cleanup);
const open = props => {render(<ToneRecovery {...props}/>); fireEvent.click(screen.getByText('Automatic tone recovery'));};
it('recovers a chosen snapshot as a preset and refreshes without automatic recall', async () => {
  const refreshed = vi.fn(); open({onRecovered:refreshed});
  const select = await screen.findByRole('combobox',{name:'Tone recovery snapshot'}); await waitFor(() => expect(select.disabled).toBe(false));
  expect(screen.getByRole('button',{name:'Add recovered preset'}).disabled).toBe(true);
  fireEvent.change(select,{target:{value:'tone-first'}}); fireEvent.click(screen.getByRole('button',{name:'Add recovered preset'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('recoverTone','tone-first'));
  expect(refreshed).toHaveBeenCalledTimes(1); expect(await screen.findByText(/Current tone preserved/)).toBeTruthy();
});
it('exposes automatic preferences and explicit snapshots and explains reference-only scope', async () => {
  open(); const toggle = await screen.findByRole('checkbox'); fireEvent.click(toggle);
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('setAutomaticRecovery',false));
  fireEvent.click(screen.getByRole('button',{name:'Snapshot current tone'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('captureRecoveryTone'));
  expect(screen.getByText(/do not contain recordings or sound files/)).toBeTruthy();
});
it('reports failures without refreshing and disables preview, plugin and busy operations', async () => {
  const refreshed = vi.fn(); bridge.invoke.mockImplementation(async name => name === 'getToneRecovery' ? bridge.status : 'Snapshot is damaged'); open({onRecovered:refreshed});
  await screen.findByRole('checkbox'); fireEvent.change(screen.getByRole('combobox'),{target:{value:'tone-first'}}); fireEvent.click(screen.getByRole('button',{name:'Add recovered preset'}));
  expect((await screen.findByRole('alert')).textContent).toContain('damaged'); expect(refreshed).not.toHaveBeenCalled();
  cleanup(); bridge.native = false; open(); expect(screen.getByRole('button',{name:'Snapshot current tone'}).disabled).toBe(true);
  cleanup(); bridge.native = true; bridge.status = {available:false,snapshots:[]}; open(); expect(screen.getByRole('button',{name:'Snapshot current tone'}).disabled).toBe(true);
  cleanup(); bridge.status = {available:true,busy:true,snapshots:[]}; open(); await screen.findByText(/Updating tone recovery/); expect(screen.getByRole('button',{name:'Snapshot current tone'}).disabled).toBe(true);
});
