import React from 'react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, within } from '@testing-library/react';
import App from './App.jsx';
import HeadSelector from './components/HeadSelector.jsx';
import { ampHeads, headForTone, headDriveControl, headSpaceControl } from './ampHeads.js';
import { startingRigs } from './startingRigs.js';
import { applyPreset } from './presets.js';
import { readParameter, setParameter } from './parameterState.js';

beforeEach(() => { localStorage.clear(); applyPreset('Glass clean'); });
afterEach(cleanup);
describe('graphical amplifier collection', () => {
  it('loads four complete voices, changes artwork, and preserves calibrated listening controls', () => {
    setParameter('INPUT_GAIN', -3); setParameter('MASTER_VOL', -21); setParameter('GUITAR_MIX_LEVEL', 4); setParameter('METRO_BPM', 117);
    render(<App/>);
    const selector = within(screen.getByRole('region', {name: 'Amplifier collection'}));
    for (const head of ampHeads) {
      fireEvent.click(selector.getByRole('button', {name: `Load ${head.name} ${head.voice.toLowerCase()} rig`}));
      expect(selector.getByRole('button', {pressed: true}).textContent).toContain(head.name);
      const amp = screen.getByRole('region', {name: 'Amplifier'});
      expect(amp.classList.contains(`head-${head.id}`)).toBe(true);
      expect(amp.querySelector('.head-art').getAttribute('src')).toBe(head.art);
      expect(within(screen.getByRole('region', {name: 'Current complete rig'})).getByText(startingRigs.find(r => r.id === head.rig).name)).toBeTruthy();
      expect(readParameter('INPUT_GAIN')).toBe(-3); expect(readParameter('MASTER_VOL')).toBe(-21);
      expect(readParameter('GUITAR_MIX_LEVEL')).toBe(4); expect(readParameter('METRO_BPM')).toBe(117);
    }
    expect(readParameter('AMP_SOURCE')).toBe(4);
    fireEvent.click(screen.getByRole('tab', {name: 'Practice'}));
    expect(screen.getByRole('region', {name: 'Compact amplifier'}).classList.contains('head-classical')).toBe(true);
    expect(screen.queryByRole('region', {name: 'Amplifier collection'})).toBeNull();
  });
  it('follows channel edits and existing crunch presets while retaining the actual amp identity', () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('button', {name: 'Load Rubicon crunch rig'}));
    expect(screen.getByRole('region', {name: 'Amplifier'}).classList.contains('head-crunch')).toBe(true);
    expect(screen.getByRole('region', {name: 'Amplifier'}).textContent).toContain('Ferrum · built-in high gain');
    fireEvent.click(screen.getByRole('button', {name: 'Channel'}));
    expect(screen.getByRole('region', {name: 'Amplifier'}).classList.contains('head-clean')).toBe(true);
    fireEvent.click(screen.getByRole('button', {name: 'Channel'}));
    expect(screen.getByRole('region', {name: 'Amplifier'}).classList.contains('head-crunch')).toBe(true);
  });
  it('disables rig loading when the caller reports an operation in progress', () => {
    const choose = vi.fn(); render(<HeadSelector head={ampHeads[0]} busy onChoose={choose}/>);
    for (const button of screen.getAllByRole('button')) { expect(button.disabled).toBe(true); fireEvent.click(button); }
    expect(choose).not.toHaveBeenCalled();
  });
  it('categorizes imported/unknown captures without claiming a new engine model', () => {
    expect(headForTone(3, false, null).id).toBe('metal');
    expect(headForTone(3, true, null).id).toBe('clean');
    expect(headForTone(1, true, startingRigs.find(r => r.id === 'factory.copper-blues')).id).toBe('crunch');
    expect(headForTone(2, false, startingRigs.find(r => r.id === 'factory.copper-blues')).id).toBe('metal');
    expect(headForTone(4, true, null, {dist: 1}, {serial: true, blocks: [{type: 'distortion', enabledId: 'dist'}]}).id).toBe('metal');
    expect(headForTone(4, true, null, {dist: 0}, {serial: true, blocks: [{type: 'distortion', enabledId: 'dist'}]}).id).toBe('classical');
    expect(headForTone(2, false, null, {DRIVE_GAIN: 0}).id).toBe('crunch');
  });
  it('routes direct metal Drive to its distortion pedal and exposes natural output trim', () => {
    const board={serial:true,blocks:[{type:'distortion',automationSlot:1,enabledId:'BOARD_DISTORTION_1_ON'}]};
    expect(headDriveControl(4,{'BOARD_DISTORTION_1_ON':1},board)).toBe('BOARD_DISTORTION_1_DIST_DRIVE');
    expect(headDriveControl(4,{'BOARD_DISTORTION_1_ON':0},board)).toBe('AMP_OUT');
    expect(headDriveControl(2,{'BOARD_DISTORTION_1_ON':1},board)).toBe('DRIVE_GAIN');
    render(<App/>); fireEvent.click(screen.getByRole('button',{name:'Load Ferrum metal rig'}));
    const drive=within(screen.getByRole('region',{name:'Amplifier'})).getByRole('slider',{name:'Drive'});
    expect(drive.max).toBe('100');
    fireEvent.change(drive,{target:{value:'71'}});
    expect(readParameter('BOARD_DISTORTION_0_DIST_DRIVE')).toBe(71);
    expect(readParameter('DRIVE_GAIN')).toBe(0);
    fireEvent.click(screen.getByRole('button',{name:'Load Aurelia classical rig'}));
    expect(within(screen.getByRole('region',{name:'Amplifier'})).getByRole('slider',{name:'Output'})).toBeTruthy();
  });
  it('edits the active space pedal and gives a dry rhythm head a working Presence control', () => {
    const board={serial:true,blocks:[{type:'spring',lane:'post',automationSlot:1,enabledId:'on'}]};
    expect(headSpaceControl({on:1},board)).toBe('BOARD_SPRING_1_SPRING_MIX');
    expect(headSpaceControl({on:0},board)).toBe('PRESENCE');
    render(<App/>); fireEvent.click(screen.getByRole('button',{name:'Load Lumen clean rig'}));
    const blend=within(screen.getByRole('region',{name:'Amplifier'})).getByRole('slider',{name:'Blend'});
    fireEvent.change(blend,{target:{value:'22'}});
    expect(readParameter('BOARD_PLATE_0_PLATE_MIX')).toBe(22);
    expect(readParameter('REVERB_MIX')).toBe(0);
    fireEvent.click(screen.getByRole('button',{name:'Load Ferrum metal rig'}));
    expect(within(screen.getByRole('region',{name:'Amplifier'})).getByRole('slider',{name:'Presence'})).toBeTruthy();
  });
});
