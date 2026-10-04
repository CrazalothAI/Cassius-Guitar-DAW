import { beforeEach, afterEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
const bridge = vi.hoisted(() => ({ invoke: vi.fn() }));
vi.mock('./juce/bridge.js', () => ({ native: true, invoke: bridge.invoke }));
import Practice, { time } from './components/Practice.jsx';
const status = {deviceSettingsAvailable: true, practice: {track: 'Backing.wav', duration: 120, position: 30, level: -12, a: 10, b: 40, recordMode: 0}};
beforeEach(() => bridge.invoke.mockReset().mockResolvedValue(''));
afterEach(cleanup);
it('uses separate transport, backing gain, loop and count-in commands', async () => {
  render(<Practice status={status} onError={vi.fn()}/>);
  fireEvent.change(screen.getByLabelText('Count-in'), {target: {value: '2'}});
  fireEvent.click(screen.getByRole('button', {name: 'Play'}));
  fireEvent.click(screen.getByRole('button', {name: 'Record guitar'}));
  fireEvent.change(screen.getByLabelText('Backing volume'), {target: {value: '-18'}});
  fireEvent.click(screen.getByRole('button', {name: /Set A/}));
  fireEvent.click(screen.getByLabelText('Loop section'));
  await waitFor(() => expect(bridge.invoke.mock.calls).toEqual([
    ['practiceStart', 'play', 2], ['practiceStart', 'record', 2], ['practiceControl', 'level', -18], ['practiceControl', 'a', 30], ['practiceControl', 'loop', 1],
  ]));
});
it('locks seeking and loading during recording while Finish take remains usable', async () => {
  render(<Practice status={{...status, practice: {...status.practice, recordMode: 3, recordSeconds: 71}}} onError={vi.fn()}/>);
  expect(screen.getByLabelText('Backing track position').disabled).toBe(true);
  expect(screen.getByLabelText('Practice speed').disabled).toBe(true);
  expect(screen.getByLabelText('Loop edge fade').disabled).toBe(true);
  expect(screen.getByRole('button', {name: 'Load backing track'}).disabled).toBe(true);
  expect(screen.getByText('Recording · 1:11')).toBeTruthy();
  fireEvent.click(screen.getByRole('button', {name: 'Finish take'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceControl', 'pause', 0));
});
it('sends pitch-preserving speed and loop fade controls with preparation feedback', async () => {
  const view = render(<Practice status={status} onError={vi.fn()}/>);
  fireEvent.change(screen.getByLabelText('Practice speed'), {target: {value: '0.75'}});
  fireEvent.change(screen.getByLabelText('Loop edge fade'), {target: {value: '10'}});
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceControl', 'speed', .75));
  expect(bridge.invoke).toHaveBeenCalledWith('practiceControl', 'fade', 10);
  view.rerender(<Practice status={{...status, practice: {...status.practice, loading: true, requestedSpeed: .75, loadProgress: .4}}} onError={vi.fn()}/>);
  expect(screen.getByRole('button', {name: 'Preparing track · 40%'}).disabled).toBe(true);
  expect(screen.getByLabelText('Practice speed').disabled).toBe(true);
  expect(screen.getByLabelText('Practice speed').value).toBe('0.75');
  expect(screen.getByRole('button', {name: 'Play'}).disabled).toBe(true);
  expect(screen.getByLabelText('Backing track position').disabled).toBe(true);
  fireEvent.click(screen.getByRole('button', {name: 'Cancel preparation'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('practiceControl', 'cancelLoad', 0));
});
it('explains DAW ownership and disables standalone actions inside a plugin', () => {
  render(<Practice status={{...status, deviceSettingsAvailable: false}} onError={vi.fn()}/>);
  expect(screen.getByRole('button', {name: 'Record guitar'}).disabled).toBe(true);
  expect(screen.getByText(/Use your DAW’s backing tracks/)).toBeTruthy();
});
it('surfaces engine failures and leaves finished take folders accessible', async () => {
  const onError = vi.fn(); bridge.invoke.mockResolvedValue('Set B after A.');
  render(<Practice status={{...status, practice: {...status.practice, takePath: 'C:/Takes/One', error: 'Disk full'}}} onError={onError}/>);
  expect(screen.getByRole('alert').textContent).toBe('Disk full');
  fireEvent.click(screen.getByLabelText('Loop section'));
  await waitFor(() => expect(onError).toHaveBeenCalledWith({title: 'Practice', text: 'Set B after A.'}));
  fireEvent.click(screen.getByRole('button', {name: 'Open take folder'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('openTakeFolder'));
  expect(time(3661)).toBe('61:01');
});
