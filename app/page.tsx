'use client';

import { motion } from 'framer-motion';
import { useRef } from 'react';
import { useScrollVideo } from '@/hooks/useScrollVideo';

export default function Home() {
  const scrollContainerRef = useRef<HTMLDivElement>(null);
  const { videoRef, scrollYProgress } = useScrollVideo(scrollContainerRef);

  return (
    <main className="min-h-screen bg-[#0A0A0E] text-white selection:bg-[#AFFE00] selection:text-black">
      {/* HERO SECTION */}
      <section className="relative h-screen flex flex-col items-center justify-center pt-20 overflow-hidden px-4 text-center">
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.8, ease: "easeOut" }}
          className="z-10"
        >
          <div className="inline-block border border-[#AFFE00] text-[#AFFE00] text-sm font-mono px-4 py-1 rounded-full mb-6 uppercase tracking-widest">
            v1.0.0 NumOS
          </div>
          <h1 className="text-5xl md:text-7xl font-bold tracking-tight mb-6">
            High-End Math,<br />
            <span className="text-transparent bg-clip-text bg-gradient-to-r from-[#00D1FF] to-[#AFFE00]">Zero-Cost Gatekeeping.</span>
          </h1>
          <p className="max-w-xl mx-auto text-gray-400 text-lg md:text-xl font-mono">
            A $15 open-source scientific calculator that outperforms $180 commercial models. Built for engineers, by engineers.
          </p>
          <div className="mt-8">
            <button className="bg-[#AFFE00] text-black font-bold py-3 px-8 rounded hover:bg-white transition-colors">
              Read the Docs
            </button>
            <button className="ml-4 border border-gray-600 bg-transparent text-white font-bold py-3 px-8 rounded hover:border-white transition-colors">
              View on GitHub
            </button>
          </div>
        </motion.div>
      </section>

      {/* BENTO GRID - HARDWARE SPECS & MOAT */}
      <section className="max-w-6xl mx-auto px-4 py-24">
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6 auto-rows-[250px]">
          {/* Card 1 */}
          <div className="md:col-span-2 bg-white/5 border border-white/10 rounded-2xl p-8 flex flex-col justify-end relative overflow-hidden group hover:border-[#00D1FF]/50 transition-colors">
            <div className="absolute inset-0 bg-gradient-to-t from-black/80 to-transparent z-10"></div>
            <div className="relative z-20">
              <h3 className="text-2xl font-bold text-[#AFFE00] mb-2">Pro-CAS Engine</h3>
              <p className="text-gray-400 font-mono text-sm max-w-md">Fully optimized DAG-based parsing and Slagle integration, delivering deep symbolic mathematics previously limited to high-end devices.</p>
            </div>
          </div>
          {/* Card 2 */}
          <div className="bg-white/5 border border-white/10 rounded-2xl p-8 flex flex-col justify-end group hover:border-[#AFFE00]/50 transition-colors">
            <h3 className="text-4xl font-mono text-white mb-1">240<span className="text-[#00D1FF] text-xl">MHz</span></h3>
            <p className="text-gray-400 text-sm">Dual-Core ESP32-S3 Extensa</p>
          </div>
          {/* Card 3 */}
          <div className="bg-white/5 border border-white/10 rounded-2xl p-8 flex flex-col justify-end group hover:border-[#AFFE00]/50 transition-colors">
            <h3 className="text-4xl font-mono text-white mb-1">8<span className="text-[#00D1FF] text-xl">MB</span></h3>
            <p className="text-gray-400 text-sm">Octal SPI PSRAM for deep stacks</p>
          </div>
          {/* Card 4 - The Moat */}
          <div className="md:col-span-2 bg-[#0A0A0E] border border-[#AFFE00]/30 rounded-2xl p-8 flex flex-col justify-end relative overflow-hidden group hover:bg-[#AFFE00]/5 transition-colors">
            <div className="relative z-20">
              <h3 className="text-2xl font-bold text-white mb-2">The Moat: 70% BoM Reduction</h3>
              <p className="text-gray-400 font-mono text-sm max-w-lg">By leveraging the ESP32-S3 over standard ARM Cortex-A configurations, NumOS offers premium capabilities without the proprietary markup, redefining hardware accessibility.</p>
            </div>
          </div>
        </div>
      </section>

      {/* SCROLL-DRIVEN VIDEO SHOWCASE */}
      <section ref={scrollContainerRef} className="h-[300vh] relative">
        <div className="sticky top-0 h-screen flex items-center justify-center overflow-hidden bg-black">
          <div className="absolute inset-0 z-10 bg-gradient-to-b from-[#0A0A0E] via-transparent to-[#0A0A0E]"></div>
          <video
            ref={videoRef}
            src="/placeholder-assembly.mp4"
            className="w-full max-w-5xl h-auto object-cover opacity-80"
            muted
            playsInline
            preload="auto"
          />
          <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 z-20 text-center pointer-events-none">
            <motion.h2 
              className="text-4xl md:text-6xl font-bold text-white drop-shadow-2xl"
              style={{ opacity: scrollYProgress }}
            >
              Unmatched Hardware.
            </motion.h2>
          </div>
        </div>
      </section>

      {/* FOOTER / REPO SYNC SECTION */}
      <footer className="border-t border-white/10 py-12 text-center text-sm font-mono text-gray-500">
        <p className="mb-4">Live synchronized with the Project Bible on GitHub.</p>
        <p>© 2026 NumOS Team. Open Source Hardware.</p>
      </footer>
    </main>
  );
}
