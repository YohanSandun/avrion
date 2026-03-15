import React from 'react';
import type { PortState } from '../types/avrion_wasm';

type PortName = 'PORTB' | 'PORTC' | 'PORTD';

interface Props {
  ports: PortState[];
  onPinToggle: (port: PortName, pin: number, high: boolean) => void;
  /** Track per-input-pin current injected level so the button renders correctly */
  inputLevels: Record<string, boolean>;
  onInputLevelChange: (port: PortName, pin: number, level: boolean) => void;
}

const portLabel: Record<string, string> = {
  PORTB: 'PORTB  (PB0–PB7)',
  PORTC: 'PORTC  (PC0–PC7)',
  PORTD: 'PORTD  (PD0–PD7)',
};

function PinWidget({
  portName, index, is_output, level, pullup,
  injectedLevel, onToggle,
}: {
  portName: PortName;
  index: number;
  is_output: boolean;
  level: boolean;
  pullup: boolean;
  injectedLevel: boolean;
  onToggle: (high: boolean) => void;
}) {
  const pinLabel = `P${portName[4]}${index}`; // e.g. "PB5"

  if (is_output) {
    // LED indicator — not interactive
    return (
      <div title={`${pinLabel} OUTPUT level=${level ? 'HIGH' : 'LOW'}`} style={{
        display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4,
      }}>
        <div style={{
          width: 24, height: 24, borderRadius: '50%',
          background: level ? '#3fb950' : '#21262d',
          border: level ? '2px solid #2ea043' : '2px solid #30363d',
          boxShadow: level ? '0 0 8px #3fb95088' : 'none',
          transition: 'background 0.15s, box-shadow 0.15s',
        }} />
        <span style={{ fontSize: 10, color: '#8b949e' }}>{pinLabel}</span>
        <span style={{ fontSize: 9, color: '#58a6ff' }}>OUT</span>
      </div>
    );
  }

  // Input toggle button
  const active = injectedLevel;
  return (
    <div title={`${pinLabel} INPUT — click to toggle injected level${pullup ? ' (pull-up)' : ''}`}
      style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4 }}>
      <button
        onClick={() => onToggle(!active)}
        style={{
          width: 24, height: 24, borderRadius: '50%', cursor: 'pointer',
          background: active ? '#e3b341' : '#161b22',
          border: active ? '2px solid #e3b341' : '2px solid #30363d',
          boxShadow: active ? '0 0 8px #e3b34188' : 'none',
          transition: 'background 0.15s, box-shadow 0.15s',
          padding: 0,
        } as React.CSSProperties}
      />
      <span style={{ fontSize: 10, color: '#8b949e' }}>{pinLabel}</span>
      <span style={{ fontSize: 9, color: '#e3b341' }}>
        {pullup ? 'IN↑' : 'IN'}
      </span>
    </div>
  );
}

export function GpioPanel({ ports, onPinToggle, inputLevels, onInputLevelChange }: Props) {
  if (ports.length === 0) {
    return (
      <div style={{ color: '#8b949e', fontSize: 13, padding: '12px 0' }}>
        Load a HEX file to see GPIO state.
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 20 }}>
      {ports.map((portState) => {
        const portName = portState.port as PortName;
        return (
          <div key={portName} style={{
            background: '#161b22',
            border: '1px solid #30363d',
            borderRadius: 8,
            padding: '14px 18px',
          }}>
            <div style={{ fontSize: 13, fontWeight: 600, color: '#c9d1d9', marginBottom: 12 }}>
              {portLabel[portName] ?? portName}
            </div>

            {/* Pin widgets */}
            <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap', justifyContent: 'flex-start' }}>
              {portState.pins.map(pin => {
                const key = `${portName}:${pin.index}`;
                return (
                  <PinWidget
                    key={pin.index}
                    portName={portName}
                    index={pin.index}
                    is_output={pin.is_output}
                    level={pin.level}
                    pullup={pin.pullup}
                    injectedLevel={inputLevels[key] ?? false}
                    onToggle={(high) => {
                      onInputLevelChange(portName, pin.index, high);
                      onPinToggle(portName, pin.index, high);
                    }}
                  />
                );
              })}
            </div>

            {/* Register summary */}
            <div style={{ marginTop: 12, display: 'flex', gap: 16, fontSize: 11, color: '#8b949e', fontFamily: 'monospace' }}>
              {(() => {
                let ddr = 0, out = 0, pin_r = 0;
                portState.pins.forEach((p, i) => {
                  if (p.is_output) ddr |= (1 << i);
                  if (p.level)     pin_r |= (1 << i);
                  // port latch reconstruction: output bit or pull-up bit
                  if (p.is_output ? p.level : p.pullup) out |= (1 << i);
                });
                const hex2 = (v: number) => v.toString(16).padStart(2, '0').toUpperCase();
                return (
                  <>
                    <span>DDR=0x{hex2(ddr)}</span>
                    <span>PORT=0x{hex2(out)}</span>
                    <span>PIN=0x{hex2(pin_r)}</span>
                  </>
                );
              })()}
            </div>
          </div>
        );
      })}
    </div>
  );
}
