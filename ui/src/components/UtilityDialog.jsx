import { useEffect, useRef } from 'react';
export default function UtilityDialog({title, onClose, notice, children}) {
  const panel = useRef(null);
  useEffect(() => { const previous = document.activeElement; panel.current?.querySelector('button')?.focus(); return () => previous?.focus(); }, []);
  return <div className="utility-overlay" onKeyDown={e => {
    if (e.key === 'Escape') { e.stopPropagation(); onClose(); }
    if (e.key === 'Tab') {
      const controls = [...panel.current.querySelectorAll('button,input,select,textarea,a[href],summary,[tabindex="0"]')].filter(x => {
        if (x.disabled || x.closest('[hidden]')) return false;
        for (let ancestor = x.parentElement; ancestor && ancestor !== panel.current; ancestor = ancestor.parentElement) {
          if (ancestor.tagName === 'DETAILS' && !ancestor.open && x !== ancestor.querySelector(':scope > summary')) return false;
        }
        return true;
      });
      const first = controls[0], last = controls.at(-1);
      if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last?.focus(); }
      else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first?.focus(); }
    }
  }}><section ref={panel} className="utility-dialog" role="dialog" aria-modal="true" aria-label={title}><div className="utility-heading"><h2>{title}</h2><button className="text-button" onClick={onClose}>Close {title.toLowerCase()}</button></div>{notice && <p className="practice-error" role="alert">{notice.title}: {notice.text}</p>}{children}</section></div>;
}
