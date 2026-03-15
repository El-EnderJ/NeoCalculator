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
      {/* Footer has been moved to global layout component */}
    </main>
  );
}
