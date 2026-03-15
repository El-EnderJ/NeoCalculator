import { Check, X } from 'lucide-react';
import Image from 'next/image';

export default function ComparePage() {
  return (
    <main className="max-w-7xl mx-auto px-4 py-24 min-h-screen">
      <div className="text-center mb-16 relative">
        <div className="absolute top-0 left-1/2 -translate-x-1/2 w-32 h-32 bg-[#ccff00]/10 blur-[50px] -z-10 rounded-full"></div>
        <h1 className="text-5xl md:text-7xl font-black tracking-tighter mb-4 text-white drop-shadow-lg">
          The Kill-Shot.
        </h1>
        <p className="text-gray-400 font-mono text-lg">NeoCalculator vs The Industry Monopolies</p>
      </div>

      <div className="overflow-x-auto pb-12 custom-scrollbar">
        <div className="min-w-[900px] grid grid-cols-5 gap-4">
          
          {/* Feature Column Header */}
          <div className="p-6 flex items-end">
             <span className="font-mono text-sm tracking-widest text-gray-500 uppercase">Feature</span>
          </div>
          
          {/* NeoCalculator Highlight Column */}
          <div className="p-6 rounded-t-2xl bg-gradient-to-b from-[#ccff00]/10 to-[#0a0a0a] border-t-2 border-x-2 border-[#ccff00] relative shadow-[0_-10px_30px_rgba(204,255,0,0.1)]">
            <div className="absolute -top-3 left-1/2 -translate-x-1/2 bg-[#ccff00] text-black text-[10px] font-black uppercase tracking-widest px-3 py-1 rounded-full whitespace-nowrap shadow-[0_0_10px_#ccff00]">
              Open Source King
            </div>
            <h3 className="text-2xl font-black text-white text-center mt-2 group">
              <span className="text-glow-lime">NeoCalculator</span>
            </h3>
            <p className="text-[#ccff00] text-center font-mono font-bold mt-2 text-xl">$15 <span className="text-sm text-[#ccff00]/70">Target BoM</span></p>
          </div>

          <div className="p-6 rounded-t-xl bg-white/5 border border-white/10 flex flex-col justify-end">
            <h3 className="text-xl font-bold text-gray-300 text-center">NumWorks</h3>
            <p className="text-gray-500 text-center font-mono mt-2">$125</p>
          </div>

          <div className="p-6 rounded-t-xl bg-white/5 border border-white/10 flex flex-col justify-end">
            <h3 className="text-xl font-bold text-gray-300 text-center">TI-84 Plus CE</h3>
            <p className="text-gray-500 text-center font-mono mt-2">$150</p>
          </div>

          <div className="p-6 rounded-t-xl bg-white/5 border border-white/10 flex flex-col justify-end">
            <h3 className="text-xl font-bold text-gray-300 text-center">HP Prime G2</h3>
            <p className="text-gray-500 text-center font-mono mt-2">$180</p>
          </div>

          {/* Row 1: CAS */}
          <div className="p-6 border-b border-white/5 flex items-center font-mono text-sm text-gray-400">Symbolic CAS</div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex items-center justify-center text-[#ccff00] font-bold"><Check className="w-5 h-5 mr-2"/> NumOS Pro-CAS</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-300">NumWorks CAS</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-red-500"><X className="w-5 h-5"/></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-green-500"><Check className="w-5 h-5 mr-2"/> Available</div>

          {/* Row 2: Performance FPS */}
          <div className="p-6 border-b border-white/5 flex items-center font-mono text-sm text-gray-400">Display FPS</div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex items-center justify-center text-white font-bold tracking-widest">60 FPS</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-400">~30 FPS</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-400">~15 FPS</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-400">~60 FPS</div>

          {/* Row 3: Processor */}
          <div className="p-6 border-b border-white/5 flex items-center font-mono text-sm text-gray-400">Processor</div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex items-center justify-center text-white font-bold text-center">ESP32-S3<br/><span className="text-xs text-gray-500">240MHz Dual-Core</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-400 text-center">STM32F427<br/><span className="text-xs text-gray-600">180MHz</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-400 text-center">eZ80<br/><span className="text-xs text-gray-600">48MHz</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-400 text-center">i.MX6<br/><span className="text-xs text-gray-600">528MHz</span></div>

          {/* Row 4: FOSS License */}
          <div className="p-6 border-b border-white/5 flex items-center font-mono text-sm text-gray-400">FOSS License</div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex items-center justify-center text-[#ccff00] font-bold"><Check className="w-5 h-5 mr-2"/> MIT</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-[#ccff00]"><Check className="w-5 h-5 mr-2"/> MIT / CC</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-red-500">Proprietary</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-red-500">Proprietary</div>

          {/* Row 5: Programmability */}
          <div className="p-6 border-b border-white/5 flex items-center font-mono text-sm text-gray-400">Programmability</div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex items-center justify-center text-center"><span className="text-white font-bold">NeoLanguage</span><br/><span className="text-xs text-[#ccff00]">Symbolic + C++</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-300">MicroPython</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-400 text-center">TI-BASIC<br/><span className="text-xs text-gray-600">+ Python</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-gray-300">HP PPL</div>

          {/* Extractor */}
          <div className="p-6 rounded-bl-2xl"></div>
          <div className="p-6 rounded-b-2xl border-b-2 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex justify-center pb-8 shadow-[0_10px_30px_rgba(204,255,0,0.05)]">
            <a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" className="bg-[#ccff00] text-black font-bold uppercase text-[10px] sm:text-xs tracking-widest px-6 py-3 rounded-full hover:bg-white transition-colors">Join NuMOS</a>
          </div>
          <div className="p-6 rounded-b-xl border-x border-b border-white/10"></div>
          <div className="p-6 rounded-b-xl border-x border-b border-white/10"></div>
          <div className="p-6 rounded-b-xl border-x border-b border-white/10"></div>
        </div>
      </div>
    </main>
  );
}
