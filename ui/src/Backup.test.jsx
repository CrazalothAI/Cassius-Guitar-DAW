import {afterEach, beforeEach, expect, it, vi} from 'vitest';
import {cleanup, fireEvent, render, screen, waitFor} from '@testing-library/react';
const bridge = vi.hoisted(() => ({invoke:vi.fn(), native:true, status:{}}));
vi.mock('./juce/bridge.js', () => ({get native() {return bridge.native;}, invoke:bridge.invoke}));
import Backup from './components/Backup.jsx';
beforeEach(() => {
  bridge.native = true; bridge.status = {available:true,busy:false};
  bridge.invoke.mockReset().mockImplementation(async name => name === 'getBackupStatus' ? bridge.status : '');
});
afterEach(cleanup);
const open = () => {render(<Backup/>); fireEvent.click(screen.getByText('Backup & recovery'));};
it('explains personal backup scope and requires a restore choice before opening the file dialog', async () => {
  open(); await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('getBackupStatus'));
  fireEvent.click(screen.getByRole('button',{name:'Restore backup'}));
  expect(screen.getByRole('group',{name:'Restore backup confirmation'}).textContent).toContain('Existing work and the current playing tone stay intact');
  expect(bridge.invoke).not.toHaveBeenCalledWith('restoreBackup');
  fireEvent.click(screen.getByRole('button',{name:'Choose backup to restore'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('restoreBackup'));
  expect(screen.queryByRole('group',{name:'Restore backup confirmation'})).toBeNull();
});
it('shows worker progress, prevents a second operation, and exposes cancellation', async () => {
  bridge.status = {available:true,busy:true,operation:'restore',progress:.45}; open();
  await screen.findByRole('progressbar'); expect(screen.getByRole('progressbar').value).toBe(.45);
  expect(screen.getByRole('button',{name:'Create backup'}).disabled).toBe(true);
  expect(screen.getByRole('button',{name:'Restore backup'}).disabled).toBe(true);
  fireEvent.click(screen.getByRole('button',{name:'Cancel operation'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('cancelBackup'));
});
it('keeps complete backups as default and explicitly requests a tone-library backup', async () => {
  open(); const toggle = screen.getByRole('checkbox',{name:'Include recorded takes and reamps in new backups'});
  expect(toggle.checked).toBe(true); fireEvent.click(toggle);
  expect(screen.getByText(/Recorded audio, take metadata and take review sections are excluded/)).toBeTruthy();
  fireEvent.click(screen.getByRole('button',{name:'Create backup'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('createBackup',false));
});
it('surfaces missing-file errors and shows the verified output', async () => {
  bridge.status = {available:true,busy:false,error:'Missing recorded audio: Take'}; open();
  expect((await screen.findByRole('alert')).textContent).toContain('Missing recorded audio');
  cleanup(); bridge.status = {available:true,busy:false,summary:'Verified backup saved',path:'C:/Backup.zip'}; open();
  await screen.findByText('Verified backup saved'); fireEvent.click(screen.getByRole('button',{name:'Show saved files'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('revealBackup'));
});
it('disables real file operations in browser preview and keeps duplicate requests out', async () => {
  bridge.native = false; open(); expect(screen.getByRole('button',{name:'Create backup'}).disabled).toBe(true);
  expect(screen.getByText(/Browser preview has no access/)).toBeTruthy(); expect(bridge.invoke).not.toHaveBeenCalled();
  cleanup(); bridge.native = true;
  let finish; bridge.invoke.mockImplementation(name => name === 'getBackupStatus' ? Promise.resolve(bridge.status) : new Promise(resolve => {finish = resolve;}));
  open(); const button = screen.getByRole('button',{name:'Create backup'}); fireEvent.click(button); fireEvent.click(button);
  expect(bridge.invoke.mock.calls.filter(([name]) => name === 'createBackup')).toHaveLength(1);
  finish('Finish recording first'); expect((await screen.findByRole('alert')).textContent).toContain('Finish recording first');
});
it('refreshes the library once when an observed restore finishes', async () => {
  const refreshed = vi.fn(); bridge.status = {available:true,busy:true,operation:'restore',progress:.5}; render(<Backup onRestored={refreshed}/>);
  await waitFor(() => expect(screen.getByRole('button',{name:'Create backup'}).disabled).toBe(true));
  bridge.status = {available:true,busy:false,operation:'restore',summary:'Recovered rigs added',path:'C:/Library/Recovered/first'};
  await waitFor(() => expect(refreshed).toHaveBeenCalledTimes(1),{timeout:2000});
});
it('refreshes after a restore that completes between polls and recognizes later recoveries', async () => {
  const refreshed = vi.fn();
  bridge.status = {available:true,busy:false,operation:'restore',path:'C:/Library/Recovered/first'};
  render(<Backup onRestored={refreshed}/>);
  await waitFor(() => expect(refreshed).toHaveBeenCalledTimes(1));
  bridge.status = {...bridge.status,path:'C:/Library/Recovered/second'};
  await waitFor(() => expect(refreshed).toHaveBeenCalledTimes(2),{timeout:2000});
  bridge.status = {...bridge.status,error:'Restore failed',path:'C:/Library/Recovered/failed'};
  await screen.findByRole('alert');
  expect(refreshed).toHaveBeenCalledTimes(2);
});
