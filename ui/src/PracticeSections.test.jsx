import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
const bridge = vi.hoisted(() => ({invoke: vi.fn()}));
vi.mock('./juce/bridge.js', () => ({invoke: bridge.invoke}));
import PracticeSections from './components/PracticeSections.jsx';
import PracticeWaveform from './components/PracticeWaveform.jsx';
const p = {duration: 120, position: 30, a: 10.25, b: 40.75, sectionRevision: 1, sections: [{id: 'solo', name: 'Solo', a: 10.25, b: 40.75}]};
beforeEach(() => { bridge.invoke.mockReset().mockResolvedValue(''); });
afterEach(cleanup);
const sections = props => <PracticeSections p={p} available={true} onError={vi.fn()} {...props}/>;
const waveform = props => <PracticeWaveform p={{...p, waveRevision: 1}} available={true} disabled={false} onSeek={vi.fn()} onError={vi.fn()} {...props}/>;
it('saves a trimmed named loop and distinguishes replacement from creation', async () => {
  render(sections()); fireEvent.click(screen.getByRole('button', {name: 'Sections · 1'}));
  expect(screen.getByRole('option', {name: 'Solo · 0:10.25–0:40.75'})).toBeTruthy();
  fireEvent.change(screen.getByLabelText('Section name'), {target: {value: '  Fast run  '}});
  fireEvent.click(screen.getByRole('button', {name: 'Save loop'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('savePracticeSection', 'Fast run', ''));
  fireEvent.change(screen.getByLabelText('Saved section'), {target: {value: 'solo'}});
  expect(screen.getByLabelText('Section name').value).toBe('Solo');
  fireEvent.change(screen.getByLabelText('Section name'), {target: {value: 'New solo'}});
  fireEvent.click(screen.getByRole('button', {name: 'Replace section'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('savePracticeSection', 'New solo', 'solo'));
});
it('recalls and deletes the selected section with explicit transport behavior', async () => {
  render(sections()); fireEvent.click(screen.getByRole('button', {name: 'Sections · 1'}));
  fireEvent.change(screen.getByLabelText('Saved section'), {target: {value: 'solo'}});
  fireEvent.click(screen.getByRole('button', {name: 'Recall section'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('recallPracticeSection', 'solo'));
  await waitFor(() => expect(screen.getByRole('button', {name: 'Delete section'}).disabled).toBe(false));
  fireEvent.click(screen.getByRole('button', {name: 'Delete section'}));
  await waitFor(() => expect(bridge.invoke).toHaveBeenCalledWith('removePracticeSection', 'solo'));
  expect(screen.getByText(/Recall pauses at A/)).toBeTruthy();
});
it('blocks controls during preparation, recording and unsupported preview modes', () => {
  const view = render(sections({disabled: true})); fireEvent.click(screen.getByRole('button', {name: 'Sections · 1'}));
  expect(screen.getByLabelText('Saved section').disabled).toBe(true);
  expect(screen.getByRole('button', {name: 'Save loop'}).disabled).toBe(true);
  view.rerender(sections({available: false})); expect(screen.getByLabelText('Section name').disabled).toBe(true);
  view.rerender(sections({p: {...p, a: 10, b: 10.01}})); expect(screen.getByRole('button', {name: 'Save loop'}).disabled).toBe(true);
});
it('refreshes deleted selections without discarding names on ordinary status polls', () => {
  const view = render(sections()); fireEvent.click(screen.getByRole('button', {name: 'Sections · 1'}));
  fireEvent.change(screen.getByLabelText('Saved section'), {target: {value: 'solo'}});
  fireEvent.change(screen.getByLabelText('Section name'), {target: {value: 'Draft name'}});
  view.rerender(sections({p: {...p, position: 40}})); expect(screen.getByLabelText('Section name').value).toBe('Draft name');
  view.rerender(sections({p: {...p, sections: [], sectionRevision: 2}}));
  expect(screen.getByLabelText('Saved section').value).toBe('');
  expect(screen.getByRole('button', {name: 'Recall section'}).disabled).toBe(true);
});
it('retains native failures and prevents duplicate submissions while saving', async () => {
  let finish; const onError = vi.fn(); bridge.invoke.mockImplementation(() => new Promise(resolve => { finish = resolve; }));
  render(sections({onError})); fireEvent.click(screen.getByRole('button', {name: 'Sections · 1'}));
  fireEvent.click(screen.getByRole('button', {name: 'Save loop'})); expect(screen.getByRole('button', {name: 'Save loop'}).disabled).toBe(true);
  finish('Could not save practice sections.');
  await waitFor(() => expect(onError).toHaveBeenCalledWith({title: 'Practice sections', text: 'Could not save practice sections.'}));
  expect(bridge.invoke).toHaveBeenCalledTimes(1);
});
it('shows corrupt storage separately and enforces names and section capacity', () => {
  render(sections({p: {...p, sectionError: 'Corrupt section document.', sections: Array.from({length: 32}, (_, i) => ({id: String(i), name: 'Part', a: 1, b: 2}))}}));
  expect(screen.getByRole('alert').textContent).toBe('Corrupt section document.');
  fireEvent.click(screen.getByRole('button', {name: 'Sections · 32'})); expect(screen.getByRole('button', {name: 'Save loop'}).disabled).toBe(true);
  fireEvent.change(screen.getByLabelText('Saved section'), {target: {value: '0'}}); expect(screen.getByRole('button', {name: 'Replace section'}).disabled).toBe(false);
  fireEvent.change(screen.getByLabelText('Section name'), {target: {value: '   '}}); expect(screen.getByRole('button', {name: 'Replace section'}).disabled).toBe(true);
});
it('draws the fetched min/max envelope and seeks by pointer and keyboard in source time', async () => {
  bridge.invoke.mockResolvedValue({revision: 1, peaks: [[-.4, .6], [0, .2]]}); const onSeek = vi.fn();
  const view = render(waveform({onSeek}));
  await waitFor(() => expect(view.container.querySelector('.wave-peaks').getAttribute('d')).toContain('M250.00,23.00V68.00'));
  const slider = screen.getByRole('slider', {name: 'Backing waveform position'});
  vi.spyOn(slider, 'getBoundingClientRect').mockReturnValue({left: 10, width: 400});
  fireEvent.click(slider, {clientX: 110}); expect(onSeek).toHaveBeenLastCalledWith(30);
  fireEvent.keyDown(slider, {key: 'ArrowRight', shiftKey: true}); expect(onSeek).toHaveBeenLastCalledWith(40);
  fireEvent.keyDown(slider, {key: 'Home'}); expect(onSeek).toHaveBeenLastCalledWith(0);
  fireEvent.keyDown(slider, {key: 'End'}); expect(onSeek).toHaveBeenLastCalledWith(120);
  fireEvent.click(slider, {clientX: 900}); expect(onSeek).toHaveBeenLastCalledWith(120);
});
it('fetches only when the waveform revision changes and ignores stale results', async () => {
  const pending = []; bridge.invoke.mockImplementation(() => new Promise(resolve => pending.push(resolve)));
  const view = render(waveform());
  view.rerender(waveform({p: {...p, waveRevision: 1, position: 40}})); expect(bridge.invoke).toHaveBeenCalledTimes(1);
  view.rerender(waveform({p: {...p, waveRevision: 2}})); expect(bridge.invoke).toHaveBeenCalledTimes(2);
  pending[0]({revision: 1, peaks: [[-1, 1]]}); pending[1]({revision: 2, peaks: [[0, .2]]});
  await waitFor(() => expect(view.container.querySelector('.wave-peaks').getAttribute('d')).toBe('M500.00,41.00V50.00'));
});
it('disables waveform seeking during takes, count-in and loading', async () => {
  const onSeek = vi.fn(); const view = render(waveform({disabled: true, onSeek}));
  const slider = screen.getByRole('slider', {name: 'Backing waveform position'});
  expect(slider.getAttribute('aria-disabled')).toBe('true'); fireEvent.keyDown(slider, {key: 'End'}); fireEvent.click(slider, {clientX: 60}); expect(onSeek).not.toHaveBeenCalled();
  view.rerender(waveform({available: false, onSeek})); expect(slider.tabIndex).toBe(-1);
});
it('reports waveform bridge failures without breaking the existing seek slider', async () => {
  const onError = vi.fn(); bridge.invoke.mockRejectedValue(new Error('Disconnected'));
  render(waveform({onError})); await waitFor(() => expect(onError).toHaveBeenCalledWith({title: 'Practice', text: 'Couldn’t read the backing waveform.'}));
  expect(screen.getByText('Waveform unavailable. Use the position slider.')).toBeTruthy();
});
