import React from 'react';
import { afterEach, describe, expect, it } from 'vitest';
import { cleanup, fireEvent, render, screen } from '@testing-library/react';
import App from './App.jsx';
afterEach(cleanup);
describe('streamlined amp editor', () => {
  it('shows six main controls and keeps rig management in the drawer', () => {
    render(<App/>);
    expect(screen.getAllByRole('slider')).toHaveLength(6);
    expect(screen.getByText(/Browser preview ·/)).toBeTruthy();
    expect(screen.queryByRole('button',{name:/Load amp model/})).toBeNull();
    fireEvent.click(screen.getByRole('button',{name:/RIG & TONE/}));
    expect(screen.getAllByRole('slider')).toHaveLength(13);
    fireEvent.click(screen.getByRole('tab',{name:'Rig'}));
    expect(screen.getByRole('button',{name:/Load amp model/}).disabled).toBe(true);
    expect(screen.getByRole('button',{name:/Load cabinet IR/}).disabled).toBe(true);
  });
  it('switches tone families without moving master volume', () => {
    render(<App/>);
    const master=screen.getByRole('slider',{name:'Master'});
    fireEvent.change(master,{target:{value:'-18'}});
    fireEvent.click(screen.getByRole('button',{name:'Clean',exact:true}));
    expect(screen.getByRole('button',{name:'Clean',exact:true}).getAttribute('aria-pressed')).toBe('true');
    expect(screen.getByRole('slider',{name:'Space'}).value).toBe('20');
    fireEvent.click(screen.getByRole('button',{name:'Metal',exact:true}));
    expect(screen.getByRole('slider',{name:'Drive'}).value).toBe('0');
    expect(master.value).toBe('-18');
    fireEvent.click(screen.getByRole('button',{name:/RIG & TONE/}));
    expect(screen.getByRole('slider',{name:'Threshold'}).value).toBe('-48');
    expect(screen.getByRole('slider',{name:'Tight'}).value).toBe('60');
    expect(screen.getByRole('slider',{name:'Output'}).value).toBe('0');
    fireEvent.click(screen.getByRole('button',{name:'Noise gate enabled'}));
    expect(screen.getByRole('button',{name:'Noise gate enabled'}).getAttribute('aria-pressed')).toBe('false');
  });
  it('retains warm cleans and separates space controls from rig controls', () => {
    render(<App/>);
    fireEvent.change(screen.getByRole('combobox'),{target:{value:'Warm clean'}});
    expect(screen.getByRole('slider',{name:'Space'}).value).toBe('14');
    fireEvent.click(screen.getByRole('button',{name:/RIG & TONE/}));
    expect(screen.getByRole('slider',{name:'Compression'}).value).toBe('55');
    fireEvent.click(screen.getByRole('tab',{name:'Space'}));
    expect(screen.getByRole('slider',{name:'Time'}).value).toBe('280');
    expect(screen.queryByRole('slider',{name:'Threshold'})).toBeNull();
  });
  it('allows calibration and reset from Shape', () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('button',{name:/RIG & TONE/}));
    const input=screen.getByRole('slider',{name:'Input'});
    fireEvent.change(input,{target:{value:'12'}});
    expect(input.value).toBe('12');
    fireEvent.doubleClick(input);
    expect(input.value).toBe('0');
  });
  it('names the chosen preset and marks it edited once a control moves', () => {
    render(<App/>);
    const lead=screen.getByRole('button',{name:'Lead',exact:true});
    fireEvent.click(lead);
    const note=()=>document.querySelector('.voice-note').textContent;
    expect(note()).toContain('Singing lead');
    expect(note()).not.toContain('Edited');
    fireEvent.change(screen.getByRole('slider',{name:'Drive'}),{target:{value:'6'}});
    expect(note()).toContain('Singing lead');
    expect(note()).toContain('Edited');
    expect(lead.getAttribute('aria-pressed')).toBe('true');
    fireEvent.click(lead);
    expect(note()).not.toContain('Edited');
  });
  it('highlights the family of presets chosen from the menu', () => {
    render(<App/>);
    fireEvent.change(screen.getByRole('combobox'),{target:{value:'Drop-Z djent'}});
    expect(screen.getByRole('button',{name:'Thall',exact:true}).getAttribute('aria-pressed')).toBe('true');
    expect(screen.getByRole('button',{name:'Metal',exact:true}).getAttribute('aria-pressed')).toBe('false');
  });
  it('moves between drawer pages with the arrow keys', () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('button',{name:/RIG & TONE/}));
    const shape=screen.getByRole('tab',{name:'Shape'});
    fireEvent.click(shape);
    fireEvent.keyDown(shape,{key:'ArrowRight'});
    expect(screen.getByRole('tab',{name:'Thall'}).getAttribute('aria-selected')).toBe('true');
    expect(document.activeElement).toBe(screen.getByRole('tab',{name:'Thall'}));
    fireEvent.keyDown(document.activeElement,{key:'End'});
    expect(screen.getByRole('tabpanel').getAttribute('aria-labelledby')).toBe('tab-Rig');
    fireEvent.keyDown(document.activeElement,{key:'ArrowRight'});
    expect(screen.getByRole('tab',{name:'Shape'}).getAttribute('aria-selected')).toBe('true');
  });
  it('opens the tuner over the amp without a detected note', () => {
    render(<App/>);
    fireEvent.click(screen.getByRole('button',{name:'TUNER'}));
    const tuner=screen.getByRole('group',{name:'Chromatic tuner'});
    expect(tuner.closest('.grille')).toBeTruthy();
    expect(tuner.className).not.toContain('in-tune');
    expect(tuner.textContent).toContain('Play a single open string');
  });
});
