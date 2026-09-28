import React from 'react';
import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render, screen } from '@testing-library/react';
vi.mock('./juce/bridge.js', () => ({
  native: true,
  slider: () => null,
  invoke: async () => ({ model: '', ir: '', input: 0, output: 0, sampleRate: 48000,
    message: 'Load failed: This capture format is unsupported.' }),
}));
import App from './App.jsx';
afterEach(cleanup);
it('shows native load failures above the signal chain', async () => {
  render(<App/>);
  const alert = await screen.findByRole('alert');
  expect(alert.textContent).toContain('This capture format is unsupported.');
});
