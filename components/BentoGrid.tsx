'use client';

import { motion, useMotionTemplate, useMotionValue } from 'framer-motion';
import { MouseEvent } from 'react';

function BentoCard({ title, description, highlight, colSpan }: { title: string, description: string, highlight?: string, colSpan?: string }) {
  const mouseX = useMotionValue(0);
  const mouseY = useMotionValue(0);

  function handleMouseMove({ currentTarget, clientX, clientY }: MouseEvent) {
    const { left, top } = currentTarget.getBoundingClientRect();
    mouseX.set(clientX - left);
    mouseY.set(clientY - top);
  }

  return (
    <div 
      className={`group relative glass-panel p-8 sm:p-10 overflow-hidden min-h-[300px] flex ${colSpan || ''}`} 
      onMouseMove={handleMouseMove}
    >
      <motion.div
        className="pointer-events-none absolute -inset-px rounded-2xl opacity-0 transition duration-300 group-hover:opacity-100"
        style={{
          background: useMotionTemplate`
            radial-gradient(
              450px circle at ${mouseX}px ${mouseY}px,
              rgba(204, 255, 0, 0.15),
              transparent 80%
            )
          `,
        }}
      />
      <div className="relative z-10 flex flex-col h-full justify-end">
        {highlight && (
          <h3 className="text-4xl md:text-5xl lg:text-6xl font-mono mb-4 text-white uppercase font-bold tracking-tighter">
            {highlight}
          </h3>
        )}
        <h4 className="text-xl md:text-2xl font-black text-[#ccff00] mb-3">{title}</h4>
        <p className="text-gray-400 text-sm md:text-base font-mono leading-relaxed max-w-lg">
          {description}
        </p>
      </div>
    </div>
  );
}

export default function BentoGrid() {
  return (
    <section className="bg-[#0a0a0a] relative z-10 py-32 px-4 sm:px-6">
      <div className="max-w-7xl mx-auto">
        <h2 className="text-3xl md:text-5xl font-black mb-16 text-center tracking-tighter font-sans text-white">
          THE <span className="text-[#ccff00]">MOAT</span>
        </h2>
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6 auto-rows-[300px]">
          <BentoCard 
            title="SRAM Limits Destroyed" 
            description="Our Pro-CAS engine fits entirely within 97KB SRAM, utilizing optimized DAG expressions for complex symbolic algebra without ever dropping performance."
            highlight="97KB RAM"
            colSpan="md:col-span-1"
          />
          <BentoCard 
            title="Radical Affordability" 
            description="Achieving a 70% BoM reduction without sacrificing computation speed. The NeoCalculator leverages bare-metal C++17 over expensive Cortex-A processors."
            highlight="$15 BoM"
            colSpan="md:col-span-2"
          />
          <BentoCard 
            title="NumOS Architecture" 
            description="A deeply integrated custom OS designed specifically for the ESP32-S3. By bypassing standard overhead, we give raw computational power directly to the hardware interrupts."
            colSpan="md:col-span-2"
          />
          <BentoCard 
            title="Fluid Interface" 
            description="Interacting with high-level mathematics shouldn't feel laggy. We maintain a strict and completely smooth UI powered by hardware SPI and LVGL."
            highlight="60 FPS"
            colSpan="md:col-span-1"
          />
        </div>
      </div>
    </section>
  );
}
