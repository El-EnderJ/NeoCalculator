export default function ManifestoPage() {
  return (
    <main className="max-w-4xl mx-auto px-6 py-24 min-h-screen">
      <div className="inline-block border border-[#ccff00]/50 text-[#ccff00] text-xs font-mono px-4 py-1 rounded-full mb-8 uppercase tracking-[0.2em] bg-[#ccff00]/5">
        The Open-Source Manifesto
      </div>
      
      <h1 className="text-5xl md:text-7xl font-black tracking-tighter mb-16 leading-[1.1] text-white selection:bg-[#ccff00] selection:text-black">
        Mathematics belongs to everyone.<br/>
        <span className="text-gray-500">Not just those who can afford $150.</span>
      </h1>

      <article className="prose prose-invert prose-lg max-w-none font-sans text-gray-300 space-y-8">
        <p className="text-xl md:text-2xl leading-relaxed font-medium text-white">
          For decades, a duopoly has controlled the educational hardware market. They have stagnated innovation, locked down firmware, and charged exorbitant prices for hardware that costs pennies to manufacture.
        </p>
        
        <p>
          The NeoCalculator project was born from a singular, radical idea: <strong>What if we could build a scientific calculator that outperforms the industry standard, for the price of a few cups of coffee?</strong>
        </p>

        <div className="my-16 pl-8 border-l-4 border-[#ccff00]">
          <h3 className="text-2xl font-bold text-white mb-4">Hardware Decolonization</h3>
          <p className="text-gray-400 font-mono text-sm leading-loose">
            By leveraging the raw, terrifying power of the ESP32-S3 microcontroller, and writing a bare-metal custom operating system (NumOS) injected directly into its veins, we achieved pure computational sovereignty.
          </p>
        </div>

        <p>
          We didn't just build a calculator. We built a platform. The Pro-CAS engine fits entirely within 97KB of SRAM, executing symbolic algebra and complex integrals faster than processors that cost 10x more. Every schematic, every line of C++, is open-source.
        </p>

        <p className="text-xl font-bold text-white mt-12">
          This isn't a product. It's a revolt. Welcome to NumOS.
        </p>
      </article>
    </main>
  );
}
