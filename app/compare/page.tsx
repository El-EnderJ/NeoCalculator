import { Check, X } from 'lucide-react';

export default function ComparePage() {
  return (
    <main className="max-w-7xl mx-auto px-4 py-24 min-h-screen">
      <div className="text-center mb-20">
        <h1 className="text-5xl md:text-7xl font-black tracking-tighter mb-6 text-white">
          The Kill-Shot.
        </h1>
        <p className="text-gray-400 font-mono text-lg">NeoCalculator vs The Industry Monopolies</p>
      </div>

      <div className="overflow-x-auto pb-12">
        <div className="min-w-[800px] grid grid-cols-4 gap-4">
          {/* Headers */}
          <div className="p-6"></div>
          
          <div className="p-6 rounded-t-2xl bg-gradient-to-b from-[#ccff00]/20 to-[#0a0a0a] border-t-2 border-x-2 border-[#ccff00] relative">
            <div className="absolute -top-3 left-1/2 -translate-x-1/2 bg-[#ccff00] text-black text-[10px] font-black uppercase tracking-widest px-3 py-1 rounded-full">
              Open Source King
            </div>
            <h3 className="text-2xl font-black text-white text-center">NeoCalculator</h3>
            <p className="text-[#ccff00] text-center font-mono font-bold mt-2">$15 BoM</p>
          </div>

          <div className="p-6 rounded-t-xl bg-white/5 border border-white/10">
            <h3 className="text-xl font-bold text-gray-300 text-center">TI-84 Plus CE</h3>
            <p className="text-gray-500 text-center font-mono mt-2">~$130 Retail</p>
          </div>

          <div className="p-6 rounded-t-xl bg-white/5 border border-white/10">
            <h3 className="text-xl font-bold text-gray-300 text-center">Casio FX-CG50</h3>
            <p className="text-gray-500 text-center font-mono mt-2">~$100 Retail</p>
          </div>

          {/* Row 1 */}
          <div className="p-6 border-b border-white/5 flex items-center"><span className="font-mono text-sm text-gray-400">Processor</span></div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex items-center justify-center"><span className="text-white font-bold">240MHz Dual-Core S3</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center"><span className="text-gray-400">48MHz eZ80</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center"><span className="text-gray-400">SuperH SH-4</span></div>

          {/* Row 2 */}
          <div className="p-6 border-b border-white/5 flex items-center"><span className="font-mono text-sm text-gray-400">SRAM</span></div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#0a0a0a] flex items-center justify-center"><span className="text-white font-bold">512KB + 8MB PSRAM</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center"><span className="text-gray-400">154KB</span></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center"><span className="text-gray-400">~16MB Flash</span></div>

          {/* Row 3 */}
          <div className="p-6 border-b border-white/5 flex items-center"><span className="font-mono text-sm text-gray-400">CAS Engine</span></div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#ccff00]/5 flex items-center justify-center">
            <span className="text-[#ccff00] font-bold flex items-center gap-2"><Check className="w-5 h-5"/> NumOS Pro-CAS</span>
          </div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-red-500"><X className="w-5 h-5"/></div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-red-500"><X className="w-5 h-5"/></div>

          {/* Row 4 */}
          <div className="p-6 border-b border-white/5 flex items-center"><span className="font-mono text-sm text-gray-400">Open Source OS / Hardware</span></div>
          <div className="p-6 border-x-2 border-[#ccff00] bg-[#ccff00]/5 flex items-center justify-center">
            <span className="text-[#ccff00] font-bold flex items-center gap-2"><Check className="w-5 h-5"/> MIT License</span>
          </div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-red-500"><X className="w-5 h-5"/> Closed</div>
          <div className="p-6 border-x border-b border-white/10 flex items-center justify-center text-red-500"><X className="w-5 h-5"/> Closed</div>

          {/* Extractor */}
          <div className="p-6 rounded-bl-2xl"></div>
          <div className="p-6 rounded-b-2xl border-b-2 border-x-2 border-[#ccff00] bg-gradient-to-t from-[#ccff00]/10 to-[#0a0a0a] flex justify-center">
            <a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" className="font-mono text-xs uppercase tracking-widest text-[#ccff00] hover:text-white transition-colors">Clone the Repo &rarr;</a>
          </div>
          <div className="p-6 rounded-b-xl border-x border-b border-white/10"></div>
          <div className="p-6 rounded-b-xl border-x border-b border-white/10"></div>
        </div>
      </div>
    </main>
  );
}
