import React from 'react';
import { beforeEach, afterEach, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
const bridge=vi.hoisted(()=>({invoke:vi.fn()}));
vi.mock('./juce/bridge.js',()=>({native:true,invoke:bridge.invoke}));
import Help, {supportReport} from './components/Help.jsx';
const status={appVersion:'1.0.0',deviceSettingsAvailable:true,sampleRate:48000,bufferSize:256,selectedInput:1,dropouts:-1,cpu:7.2,overruns:2,board:{serial:true,blocks:[{},{}]},audioDevice:{driver:'ASIO',inputDevice:'USB interface',outputDevice:'USB interface',monitoring:false}};
beforeEach(()=>bridge.invoke.mockReset().mockResolvedValue(''));
afterEach(cleanup);
it('explains first sound with built-in rigs and explicit input monitoring',async()=>{
  render(<Help status={status} onError={vi.fn()}/>);
  expect(screen.getByText(/All 22 built-in rigs work without extra files/)).toBeTruthy();
  expect(screen.getByLabelText('Monitor guitar input').checked).toBe(false);
  fireEvent.click(screen.getByLabelText('Monitor guitar input'));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('setInputMonitoring',true));
  fireEvent.click(screen.getByRole('button',{name:'Open audio settings'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('showAudioSettings'));
});
it('keeps DAW monitoring with the host and does not invent standalone controls',()=>{
  render(<Help status={{...status,deviceSettingsAvailable:false,audioDevice:undefined,selectedInput:undefined}} onError={vi.fn()}/>);
  expect(screen.getByText(/enable monitoring on its Cassian track/)).toBeTruthy();
  expect(screen.queryByLabelText('Monitor guitar input')).toBeNull();
  expect(screen.getByLabelText('Support report').value).toContain('Mode: DAW plugin');
});
it('copies a reviewed report while excluding recordings, paths and sound names',async()=>{
  const privateStatus={...status,model:'Private amp',message:'C:\\private\\guitar.wav',takes:{path:'Secret recording'},midiInputs:[{id:'private-device-id'}]};
  render(<Help status={privateStatus} onError={vi.fn()}/>);
  const report=screen.getByLabelText('Support report').value;
  expect(report).toContain('Input channel: 2');expect(report).toContain('Device dropouts: Not reported by driver');
  for(const secret of ['Private amp','private-device-id','Secret recording','private\\guitar'])expect(report).not.toContain(secret);
  expect(bridge.invoke).not.toHaveBeenCalled();
  fireEvent.click(screen.getByRole('button',{name:'Copy support report'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('copySupportReport',report));
  expect(await screen.findByText(/Copied. Paste it/)).toBeTruthy();
});
it('reports native failures without presenting an unsuccessful copy as success',async()=>{
  bridge.invoke.mockResolvedValue('Clipboard unavailable.');const error=vi.fn();
  render(<Help status={status} onError={error}/>);fireEvent.click(screen.getByRole('button',{name:'Copy support report'}));
  await waitFor(()=>expect(error).toHaveBeenCalledWith({title:'Help & setup',text:'Clipboard unavailable.'}));
  expect(screen.queryByText(/Copied. Paste it/)).toBeNull();
});
it('opens only named external help links through the native bridge',async()=>{
  render(<Help status={status} onError={vi.fn()}/>);fireEvent.click(screen.getByRole('link',{name:'User guide'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('openHelpLink','guide'));
  expect(screen.getByRole('link',{name:'License'}).href).toContain('/LICENSE.txt');
});
it('keeps unknown input and unavailable rates explicit in support reports',()=>{
  const report=supportReport({deviceSettingsAvailable:false,selectedInput:-1,dropouts:0});
  expect(report).toContain('Input channel: Controlled by host / unavailable');expect(report).toContain('Sample rate: Unavailable Hz');expect(report).toContain('Device dropouts: 0');
});
