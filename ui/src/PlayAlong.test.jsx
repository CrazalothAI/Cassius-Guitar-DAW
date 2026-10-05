import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
const bridge = vi.hoisted(() => ({invoke: vi.fn(), set: vi.fn()}));
vi.mock('./juce/bridge.js', () => ({native: true, invoke: bridge.invoke}));
vi.mock('./parameterState.js', () => ({useParameter: () => 0, setParameter: bridge.set, beginGesture: vi.fn(), endGesture: vi.fn()}));
import PlayAlong from './components/PlayAlong.jsx';
beforeEach(() => { bridge.invoke.mockReset().mockResolvedValue(''); bridge.set.mockReset(); });
afterEach(cleanup);
it('offers repeatable listening changes and a neutral reset without touching the tone', () => {
  render(<PlayAlong status={{deviceSettingsAvailable: true}} onError={vi.fn()}/>);
  fireEvent.click(screen.getByRole('button', {name: 'Bring guitar forward'}));
  fireEvent.click(screen.getByRole('button', {name: 'Bring guitar forward'}));
  expect(bridge.set.mock.calls).toEqual([['GUITAR_MIX_LEVEL', 3], ['GUITAR_MIX_FOCUS', 45], ['GUITAR_MIX_LEVEL', 3], ['GUITAR_MIX_FOCUS', 45]]);
  fireEvent.click(screen.getByRole('button', {name: 'Neutral mix'}));
  expect(bridge.set.mock.calls.slice(-2)).toEqual([['GUITAR_MIX_LEVEL', 0], ['GUITAR_MIX_FOCUS', 0]]);
  expect(screen.getByText(/Cassian cannot change browser audio/)).toBeTruthy();
});
it('reports a held peak threshold without claiming measured limiter reduction', () => {
  const view = render(<PlayAlong status={{outputPeakWarning: true}} onError={vi.fn()}/>);
  expect(screen.getByRole('alert').textContent).toMatch(/−0.5 dBFS.*last second/);
  expect(screen.getByRole('alert').textContent).not.toMatch(/reducing|gain reduction/);
  view.rerender(<PlayAlong status={{outputPeakWarning: false}} onError={vi.fn()}/>);
  expect(screen.queryByRole('alert')).toBeNull();
});
it('adjusts only a loaded Cassian track and surfaces native failures', async () => {
  const error = vi.fn(); bridge.invoke.mockResolvedValueOnce('Track unavailable');
  const view = render(<PlayAlong status={{deviceSettingsAvailable: true, practice: {duration: 40, level: -18}}} onError={error}/>);
  fireEvent.change(screen.getByLabelText('Cassian backing volume'), {target: {value: '-24'}});
  await waitFor(() => expect(error).toHaveBeenCalledWith({title: 'Backing volume', text: 'Track unavailable'}));
  expect(bridge.invoke).toHaveBeenCalledWith('practiceControl', 'level', -24);
  bridge.invoke.mockRejectedValueOnce(new Error('disconnect'));
  fireEvent.change(screen.getByLabelText('Cassian backing volume'), {target: {value: '-22'}});
  await waitFor(() => expect(error).toHaveBeenLastCalledWith({title: 'Backing volume', text: expect.stringMatching(/interrupted/)}));
  view.rerender(<PlayAlong status={{deviceSettingsAvailable: false}} onError={error}/>);
  expect(screen.queryByLabelText('Cassian backing volume')).toBeNull();
  expect(screen.getByText(/host mixer/)).toBeTruthy();
});
