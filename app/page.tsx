import HeroCanvas from '@/components/HeroCanvas';
import BentoGrid from '@/components/BentoGrid';
import HardwareScroll from '@/components/HardwareScroll';

export default function Home() {
  return (
    <main className="min-h-screen bg-[#0a0a0a] text-white selection:bg-[#ccff00] selection:text-black font-sans overflow-x-hidden">
      <HeroCanvas />
      
      {/* TRANSITION SEPERATOR */}
      <div className="h-32 bg-[#0a0a0a] border-t border-white/5 relative z-10 w-full flex justify-center items-center">
        <div className="w-px h-16 bg-gradient-to-b from-[#ccff00]/50 to-transparent"></div>
      </div>

      {/* GSAP Scroll Sequence */}
      <div className="relative z-20">
        <HardwareScroll />
      </div>

      <div className="relative z-30">
        <BentoGrid />
      </div>
      
      {/* INVESTOR FOOTER */}
      <footer className="py-24 border-t border-white/10 text-center relative z-30 bg-[#0a0a0a] overflow-hidden">
        {/* Glow behind footer */}
        <div className="absolute left-1/2 top-0 -translate-x-1/2 -translate-y-1/2 w-[600px] h-[300px] bg-[#ccff00]/10 blur-[100px] rounded-full pointer-events-none"></div>
        
        <h2 className="text-4xl md:text-5xl lg:text-6xl font-black mb-8 tracking-tighter font-sans text-white">
          Join the Resistance.
        </h2>
        <div className="flex flex-col sm:flex-row gap-6 justify-center max-w-2xl mx-auto px-6">
          <a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" rel="noopener noreferrer" className="bg-[#ccff00] text-black font-black py-4 px-8 md:px-10 rounded-lg hover:bg-white transition-colors duration-300 flex items-center justify-center uppercase tracking-[0.15em] text-xs md:text-sm">
            View on GitHub
          </a>
          <a href="mailto:el.enderj2020@gmail.com" className="border border-white/20 bg-black/50 backdrop-blur-md text-white font-bold py-4 px-8 md:px-10 rounded-lg hover:border-white/60 hover:bg-white/10 transition-all duration-300 flex items-center justify-center uppercase tracking-[0.15em] text-xs md:text-sm shadow-[0_0_20px_rgba(0,0,0,0.5)]">
            Investment Inquiry
          </a>
        </div>
        <div className="mt-20 text-xs font-mono text-gray-600 tracking-widest uppercase">
          <p className="mb-2">Hardware Decolonization starts here.</p>
          <p>© {new Date().getFullYear()} NumOS Open-Source Alliance.</p>
        </div>
      </footer>
    </main>
  );
}
