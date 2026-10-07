import {beforeEach,afterEach,expect,it,vi} from 'vitest';
import {cleanup,fireEvent,render,screen,waitFor,within} from '@testing-library/react';
const bridge=vi.hoisted(()=>({invoke:vi.fn(),rigs:[]}));
vi.mock('./juce/bridge.js',()=>({native:true,invoke:bridge.invoke}));
import Library from './components/Library.jsx';
beforeEach(()=>{
  bridge.rigs=[{id:'saved',name:'My clean',styles:'jazz',gain:'clean',notes:'Original notes'}];
  bridge.invoke.mockReset().mockImplementation(async name=>name==='getLibrary'?{assets:[],rigs:bridge.rigs}:'');
});
afterEach(cleanup);
async function open(props={}) {
  const view=render(<Library onClose={vi.fn()} {...props}/>);
  fireEvent.click(screen.getByRole('button',{name:'Presets'})); await screen.findByText('My clean');
  fireEvent.click(screen.getByText('My clean')); return view;
}
it('sends metadata separately from saved tone, and duplicates without loading a rig',async()=>{
  await open();
  fireEvent.change(screen.getByLabelText('Saved rig name'),{target:{value:' Jazz lead '}});
  fireEvent.change(screen.getByLabelText('Rig styles (comma separated)'),{target:{value:'Jazz, LEAD, jazz'}});
  fireEvent.change(screen.getByLabelText('Rig notes'),{target:{value:'For the next song'}});
  fireEvent.click(screen.getByRole('button',{name:'Save rig metadata'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('editRig','saved',{name:'Jazz lead',styles:'jazz, lead',gain:'clean',tags:'',notes:'For the next song'}));
  await waitFor(()=>expect(screen.getByRole('button',{name:'Duplicate rig My clean'}).disabled).toBe(false));
  fireEvent.click(screen.getByRole('button',{name:'Duplicate rig My clean'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('duplicateRig','saved','My clean copy'));
  expect(bridge.invoke.mock.calls.some(([name])=>['loadRig','saveRig','applyRig'].includes(name))).toBe(false);
});
it('blocks duplicate requests while busy and surfaces persistence errors',async()=>{
  let finish; await open();
  bridge.invoke.mockImplementation(name=>name==='getLibrary'?Promise.resolve({assets:[],rigs:bridge.rigs}):new Promise(resolve=>{finish=resolve;}));
  const button=screen.getByRole('button',{name:'Duplicate rig My clean'}); fireEvent.click(button); fireEvent.click(button);
  expect(bridge.invoke.mock.calls.filter(([name])=>name==='duplicateRig')).toHaveLength(1);
  expect(screen.getByRole('button',{name:'Save rig metadata'}).disabled).toBe(true);
  finish('Could not save the shared library');
  await screen.findByRole('alert'); expect(screen.getByRole('alert').textContent).toBe('Could not save the shared library');
  expect(button.disabled).toBe(false);
});
it('disables saved-rig editing and duplication during loading and routes favorites through rig metadata',async()=>{
  const {rerender}=await open({loading:true});
  expect(screen.getByRole('button',{name:'Duplicate rig My clean'}).disabled).toBe(true);
  expect(screen.getByRole('button',{name:'Save rig metadata'}).disabled).toBe(true);
  expect(screen.getByLabelText('Saved rig name').disabled).toBe(true);
  rerender(<Library onClose={vi.fn()} loading={false}/>);
  fireEvent.click(screen.getByRole('button',{name:'Favorite My clean'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('editRig','saved',{favorite:true}));
});
it('shows missing rig dependencies, relinks them and restores availability without loading',async()=>{
  let missing=true;
  bridge.invoke.mockImplementation(async(name,...args)=>{
    if(name==='getLibrary') return {assets:[],rigs:bridge.rigs};
    if(name==='inspectRig') return {assets:[{stage:'model',id:'amp:original',name:'Lead capture',missing,canRelink:true}]};
    if(name==='relinkAsset') {missing=false;return true;}
    return '';
  });
  await open();
  await screen.findByRole('button',{name:'Relink Lead capture'});
  const row=screen.getByRole('button',{name:'Favorite My clean'}).closest('article');
  expect(within(row).getByRole('button',{name:'Use'}).disabled).toBe(true);
  fireEvent.click(screen.getByRole('button',{name:'Relink Lead capture'}));
  await waitFor(()=>expect(bridge.invoke).toHaveBeenCalledWith('relinkAsset','amp:original'));
  await waitFor(()=>expect(screen.queryByRole('button',{name:'Relink Lead capture'})).toBeNull());
  expect(within(row).getByRole('button',{name:'Use'}).disabled).toBe(false);
  expect(bridge.invoke.mock.calls.some(([name])=>name==='loadRig')).toBe(false);
});
it('surfaces invalid saved rig inspection without allowing recall',async()=>{
  bridge.invoke.mockImplementation(async name=>name==='getLibrary'?{assets:[],rigs:bridge.rigs}:name==='inspectRig'?{error:'Invalid saved rig.'}:'');
  await open(); await screen.findByText('Invalid saved rig.');
  const row=screen.getByRole('button',{name:'Favorite My clean'}).closest('article');
  expect(within(row).getByRole('button',{name:'Use'}).disabled).toBe(true);
});
