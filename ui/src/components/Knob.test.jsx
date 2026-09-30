import React from 'react';
import { afterEach, beforeAll, describe, expect, it, vi } from 'vitest';
import { act, cleanup, fireEvent, render, screen } from '@testing-library/react';
const mock = vi.hoisted(() => {
  const event = () => {
    const listeners = new Set();
    return { listeners, addListener: f => { listeners.add(f); return f; }, removeListener: f => listeners.delete(f), emit: () => listeners.forEach(f => f()) };
  };
  // Like JUCE's getSliderState, the bridge hands out one cached state object per parameter.
  const state = { value: 6, valueChangedEvent: event(), propertiesChangedEvent: event(), setNormalisedValue: vi.fn(), sliderDragStarted: vi.fn(), sliderDragEnded: vi.fn() };
  state.getScaledValue = () => state.value;
  return state;
});
vi.mock('../juce/bridge.js', () => ({ slider: () => mock }));
import Knob from './Knob.jsx';
afterEach(() => { cleanup(); vi.clearAllMocks(); });
// jsdom has no PointerEvent; MouseEvent carries the coordinates and modifiers the knob reads.
beforeAll(() => { window.PointerEvent ??= class extends MouseEvent { constructor(type, init = {}) { super(type, init); this.pointerId = init.pointerId ?? 1; } }; });
const dial = () => screen.getByRole('slider').closest('.dial');
describe('native parameter attachment', () => {
  it('shows host automation and removes subscriptions when closed', () => {
    const { unmount } = render(<Knob id="INPUT_GAIN"/>);
    expect(screen.getByRole('slider').value).toBe('6');
    act(() => { mock.value = -3; mock.valueChangedEvent.emit(); });
    expect(screen.getByRole('slider').value).toBe('-3');
    unmount();
    expect(mock.valueChangedEvent.listeners.size).toBe(0);
    expect(mock.propertiesChangedEvent.listeners.size).toBe(0);
  });
  it('normalizes values and brackets keyboard changes with host gestures', () => {
    render(<Knob id="INPUT_GAIN"/>);
    const input = screen.getByRole('slider');
    fireEvent.keyDown(input, {key: 'ArrowRight'});
    fireEvent.change(input, {target: {value: 12}});
    fireEvent.keyUp(input, {key: 'ArrowRight'});
    expect(mock.setNormalisedValue).toHaveBeenCalledWith(0.75);
    expect(mock.sliderDragStarted).toHaveBeenCalledTimes(1);
    expect(mock.sliderDragEnded).toHaveBeenCalledTimes(1);
  });
});
describe('knob interaction', () => {
  it('never jumps on press and drags relative to the starting value', () => {
    mock.value = 0;
    render(<Knob id="INPUT_GAIN"/>);
    fireEvent.pointerDown(dial(), { button: 0, clientX: 50, clientY: 100 });
    fireEvent.pointerUp(dial());
    expect(mock.setNormalisedValue).not.toHaveBeenCalled();
    expect(mock.sliderDragStarted).not.toHaveBeenCalled();
    fireEvent.pointerDown(dial(), { button: 0, clientX: 50, clientY: 100 });
    fireEvent.pointerMove(dial(), { clientX: 50, clientY: 75 });
    expect(screen.getByRole('slider').value).toBe('6');
    fireEvent.pointerMove(dial(), { clientX: 50, clientY: 65, shiftKey: true });
    fireEvent.pointerUp(dial());
    expect(mock.setNormalisedValue).toHaveBeenLastCalledWith((6.2 + 24) / 48);
    expect(mock.sliderDragStarted).toHaveBeenCalledTimes(1);
    expect(mock.sliderDragEnded).toHaveBeenCalledTimes(1);
  });
  it('steps 1% per arrow, one step with Shift and 10% per page within one held gesture', () => {
    mock.value = 0;
    render(<Knob id="INPUT_GAIN" small/>);
    const input = screen.getByRole('slider');
    fireEvent.keyDown(input, { key: 'ArrowUp' });
    expect(input.value).toBe('0.5');
    fireEvent.keyDown(input, { key: 'ArrowDown', shiftKey: true });
    expect(input.value).toBe('0.4');
    fireEvent.keyDown(input, { key: 'PageDown' });
    expect(input.value).toBe('-4.4');
    fireEvent.keyDown(input, { key: 'End' });
    expect(input.getAttribute('aria-valuetext')).toBe('+24.0 dB');
    fireEvent.keyUp(input, { key: 'End' });
    expect(mock.sliderDragStarted).toHaveBeenCalledTimes(1);
    expect(mock.sliderDragEnded).toHaveBeenCalledTimes(1);
  });
  it('puts the musical centre of wide ranges at mid-travel', () => {
    mock.value = 8000;
    render(<Knob id="HIGH_CUT" small/>);
    expect(dial().style.getPropertyValue('--angle')).toBe('0deg');
    const input = screen.getByRole('slider');
    fireEvent.keyDown(input, { key: 'ArrowDown' });
    const oneStepDown = Number(input.value);
    expect(oneStepDown).toBeLessThan(8000);
    expect(oneStepDown).toBeGreaterThan(7800);
    fireEvent.keyUp(input, { key: 'ArrowDown' });
    // The host receives the same skewed normalised value JUCE computes.
    const skew = Math.log(0.5) / Math.log(5000 / 17000);
    expect(mock.setNormalisedValue).toHaveBeenLastCalledWith(((oneStepDown - 3000) / 17000) ** skew);
    fireEvent.keyDown(input, { key: 'End' });
    expect(input.getAttribute('aria-valuetext')).toBe('Off');
  });
  it('resets on double-click and fills centre-detented arcs from zero', () => {
    mock.value = 6;
    const { container } = render(<Knob id="AMP_BASS"/>);
    expect(container.querySelector('output').textContent).toBe('+6.0 dB');
    expect(dial().style.getPropertyValue('--from')).toBe('135deg');
    expect(dial().style.getPropertyValue('--to')).toBe('202.5deg');
    fireEvent.doubleClick(dial());
    expect(screen.getByRole('slider').value).toBe('0');
    expect(mock.setNormalisedValue).toHaveBeenLastCalledWith(0.5);
  });
});
