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
    fireEvent.click(screen.getByRole('button',{name:'Rig',exact:true}));
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
    expect(screen.getByRole('slider',{name:'Noise gate'}).value).toBe('-48');
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
    fireEvent.click(screen.getByRole('button',{name:'Space',exact:true}));
    expect(screen.getByRole('slider',{name:'Time'}).value).toBe('280');
    expect(screen.queryByRole('slider',{name:'Noise gate'})).toBeNull();
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
});
