import React from 'react';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { afterEach, expect, it, vi } from 'vitest';
import StarterTour from './components/StarterTour.jsx';
import catalog from './startingRigs.json';
afterEach(cleanup);
function open(props) { const view=render(<StarterTour status={{}} {...props}/>); fireEvent.click(screen.getByText('Audition original starter tones')); return view; }
it('offers contrasting file-free rigs with clean as the first choice',()=>{
  open({onChooseRig:vi.fn()});
  const select=screen.getByRole('combobox',{name:'Starter sound to try'});
  expect(select.value).toBe('factory.prism-clean');
  expect(select.options).toHaveLength(6);
  for(const option of select.options) {
    const rig=catalog.rigs.find(r=>r.id===option.value);
    expect(rig).toBeTruthy(); expect(rig.assets).toBeUndefined();
  }
  expect(screen.getByText(/Save your edits first/)).toBeTruthy();
  expect(screen.getByText(/Input trim, Master and Play Along mix stay/)).toBeTruthy();
  fireEvent.change(select,{target:{value:'factory.natural-nylon'}});
  expect(screen.getByText(/does not turn an electric guitar into a nylon guitar/)).toBeTruthy();
});
it('loads only an explicitly selected original complete rig and prevents duplicate requests',async()=>{
  let finish;const load=vi.fn(()=>new Promise(resolve=>{finish=resolve;}));open({onChooseRig:load});
  expect(load).not.toHaveBeenCalled();
  fireEvent.change(screen.getByRole('combobox',{name:'Starter sound to try'}),{target:{value:'factory.velvet-lead'}});
  fireEvent.click(screen.getByRole('button',{name:'Load Velvet Lead and open Tone'}));
  expect(load.mock.calls).toEqual([['factory.velvet-lead']]);
  expect(screen.getByRole('button',{name:'Loading starter…'}).disabled).toBe(true);
  finish(true);await waitFor(()=>expect(screen.getByRole('button',{name:'Load Velvet Lead and open Tone'}).disabled).toBe(false));
});
it('blocks recording, review playback and an already loading rig',()=>{
  for(const status of [{practice:{recordMode:2}},{review:{playing:true}},{rigLoading:true}]) {
    const load=vi.fn();const view=open({status,onChooseRig:load});
    fireEvent.click(screen.getByRole('button',{name:'Load Prism Clean and open Tone'}));
    expect(load).not.toHaveBeenCalled();view.unmount();
  }
});
it('reports a failed load without inventing a successful result',async()=>{
  const error=vi.fn();open({onChooseRig:vi.fn().mockRejectedValue(new Error('Could not prepare tone.')),onError:error});
  fireEvent.click(screen.getByRole('button',{name:'Load Prism Clean and open Tone'}));
  await waitFor(()=>expect(error).toHaveBeenCalledWith({title:'Starter tones',text:'Could not prepare tone.'}));
});
