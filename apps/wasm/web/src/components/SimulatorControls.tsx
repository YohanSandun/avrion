import React from 'react';
import type { SimStatus } from '../hooks/useSimulator';

interface Props {
  status: SimStatus;
  totalCycles: number;
  hexLoaded: boolean;
  onReset: () => void;
  onStart: () => void;
  onPause: () => void;
  onStep: (n: number) => void;
}

const btn = (extra: React.CSSProperties = {}): React.CSSProperties => ({
  padding: '6px 16px',
  borderRadius: 6,
  border: '1px solid #30363d',
  background: '#21262d',
  color: '#c9d1d9',
  cursor: 'pointer',
  fontSize: 13,
  fontWeight: 500,
  ...extra,
});

const STATUS_COLORS: Record<SimStatus, string> = {
  loading: '#8b949e',
  idle:    '#8b949e',
  ready:   '#3fb950',
  running: '#58a6ff',
  paused:  '#e3b341',
  error:   '#f85149',
};

export function SimulatorControls({
  status, totalCycles, hexLoaded,
  onReset, onStart, onPause, onStep,
}: Props) {
  const isRunning = status === 'running';
  const canRun    = hexLoaded && (status === 'ready' || status === 'paused');
  const canStep   = hexLoaded && !isRunning;

  const fmtCycles = (n: number) => {
    if (n >= 1_000_000) return `${(n / 1_000_000).toFixed(2)} Mcycles`;
    if (n >= 1_000)     return `${(n / 1_000).toFixed(1)} kcycles`;
    return `${Math.round(n)} cycles`;
  };

  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 10, flexWrap: 'wrap' }}>
      {/* Status badge */}
      <span style={{
        background: STATUS_COLORS[status] + '22',
        color: STATUS_COLORS[status],
        border: `1px solid ${STATUS_COLORS[status]}55`,
        borderRadius: 99,
        padding: '3px 12px',
        fontSize: 12,
        fontWeight: 600,
        letterSpacing: '0.4px',
        textTransform: 'uppercase',
        minWidth: 72,
        textAlign: 'center',
      }}>
        {status}
      </span>

      {/* Cycle counter */}
      <span style={{ fontSize: 13, color: '#8b949e', minWidth: 110 }}>
        {fmtCycles(totalCycles)}
      </span>

      {/* Controls */}
      <button style={btn()} onClick={onReset} disabled={status === 'loading'}>
        ↺ Reset
      </button>

      {isRunning ? (
        <button style={btn({ background: '#e3b341', color: '#0d1117', borderColor: '#e3b341' })} onClick={onPause}>
          ⏸ Pause
        </button>
      ) : (
        <button
          style={btn(canRun ? { background: '#238636', color: '#fff', borderColor: '#2ea043' } : {})}
          onClick={onStart}
          disabled={!canRun}
        >
          ▶ Run
        </button>
      )}

      <button
        style={btn()}
        onClick={() => onStep(1000)}
        disabled={!canStep}
        title="Step ~1k cycles"
      >
        ⏭ Step 1k
      </button>

      <button
        style={btn()}
        onClick={() => onStep(16000)}
        disabled={!canStep}
        title="Step ~16k cycles"
      >
        ⏭ Step 16k
      </button>
    </div>
  );
}
