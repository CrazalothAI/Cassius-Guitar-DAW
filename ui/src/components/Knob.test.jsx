import React from 'react';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { act, cleanup, fireEvent, render, screen } from '@testing-library/react';
const mock = vi.hoisted(() => {
  const event = () => {
    const listeners = new Set();
    return { listeners, addListener: f => { listeners.add(f); return f; }, removeListener: f => listeners.delete(f), emit: () => listeners.forEach(f => f()) };
  };
  return { value: 6, valueChangedEvent: event(), propertiesChangedEvent: event(), setNormalisedValue: vi.fn(), sliderDragStarted: vi.fn(), sliderDragEnded: vi.fn() };
});
vi.mock('../juce/bridge.js', () => ({ slider: () => ({ ...mock, getScaledValue: () => mock.value }) }));
import Knob from './Knob.jsx';
afterEach(() => { cleanup(); vi.clearAllMocks(); });
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
