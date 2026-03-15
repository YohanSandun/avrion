import React, { useState } from 'react';
import { useSimulator } from './hooks/useSimulator';
import { HexUploader } from './components/HexUploader';
import { SimulatorControls } from './components/SimulatorControls';
import { GpioPanel } from './components/GpioPanel';

type PortName = 'PORTB' | 'PORTC' | 'PORTD';

export default function App() {
  const sim = useSimulator();
  // Track per-(port,pin) injected input level locally for accurate button rendering
  const [inputLevels, setInputLevels] = useState<Record<string, boolean>>({});

  const handleInputLevelChange = (port: PortName, pin: number, level: boolean) => {
    setInputLevels(prev => ({ ...prev, [`${port}:${pin}`]: level }));
  };

  return (
    <div style={{ maxWidth: 900, margin: '0 auto', padding: '32px 20px' }}>
      {/* Header */}
      <div style={{ marginBottom: 28 }}>
        <h1 style={{ fontSize: 22, fontWeight: 700, color: '#e6edf3', letterSpacing: '-0.3px' }}>
          Avrion — WebAssembly GPIO Tester
        </h1>
        <p style={{ fontSize: 13, color: '#8b949e', marginTop: 6 }}>
          ATmega328P cycle-accurate simulator running in the browser.
        </p>
      </div>

      {/* Loading / Error banner */}
      {sim.status === 'loading' && (
        <div style={banner('#58a6ff')}>
          ⏳ Loading WebAssembly simulator module…
        </div>
      )}
      {sim.status === 'error' && (
        <div style={banner('#f85149')}>
          ❌ Simulator error: {sim.errorMsg}
          <div style={{ fontSize: 11, marginTop: 4, opacity: 0.8 }}>
            Make sure <code>avrion_wasm.js</code> and <code>avrion_wasm.wasm</code> are
            in <code>apps/wasm/web/public/</code> (run the Emscripten build first).
          </div>
        </div>
      )}

      {/* HEX file uploader */}
      <section style={section}>
        <SectionTitle>1. Load Program</SectionTitle>
        <HexUploader onLoad={sim.loadHex} disabled={sim.status === 'loading' || sim.status === 'error'} />
      </section>

      {/* Simulator controls */}
      <section style={section}>
        <SectionTitle>2. Run Simulation</SectionTitle>
        <SimulatorControls
          status={sim.status}
          totalCycles={sim.totalCycles}
          hexLoaded={sim.hexLoaded}
          onReset={sim.reset}
          onStart={sim.start}
          onPause={sim.pause}
          onStep={sim.step}
        />
      </section>

      {/* GPIO panel */}
      <section style={section}>
        <SectionTitle>3. GPIO State</SectionTitle>
        <p style={{ fontSize: 12, color: '#8b949e', marginBottom: 14 }}>
          <span style={{ color: '#3fb950' }}>●</span> Green LED = output HIGH &nbsp;|&nbsp;
          <span style={{ color: '#8b949e' }}>● </span>Dark = output LOW &nbsp;|&nbsp;
          <span style={{ color: '#e3b341' }}>●</span> Yellow button = input (click to toggle injected level)
        </p>
        <GpioPanel
          ports={sim.ports}
          onPinToggle={sim.setPinInput}
          inputLevels={inputLevels}
          onInputLevelChange={handleInputLevelChange}
        />
      </section>
    </div>
  );
}

// ---- helpers ----------------------------------------------------------------

const section: React.CSSProperties = {
  marginBottom: 28,
  borderTop: '1px solid #21262d',
  paddingTop: 22,
};

function SectionTitle({ children }: { children: React.ReactNode }) {
  return (
    <h2 style={{ fontSize: 13, fontWeight: 600, color: '#8b949e', textTransform: 'uppercase', letterSpacing: '0.8px', marginBottom: 14 }}>
      {children}
    </h2>
  );
}

function banner(color: string): React.CSSProperties {
  return {
    background: color + '18',
    border: `1px solid ${color}44`,
    borderRadius: 6,
    padding: '10px 16px',
    fontSize: 13,
    color,
    marginBottom: 20,
  };
}
