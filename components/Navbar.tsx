import Link from 'next/link';
import { Calculator } from 'lucide-react';

export default function Navbar() {
  return (
    <header className="fixed top-0 left-0 right-0 z-50 border-b border-white/10 bg-[#0a0a0a]/70 backdrop-blur-xl supports-[backdrop-filter]:bg-[#0a0a0a]/60">
      <div className="max-w-7xl mx-auto px-4 h-16 flex items-center justify-between">
        <Link href="/" className="flex items-center gap-2 group">
          <Calculator className="w-6 h-6 text-[#ccff00] transition-transform group-hover:rotate-12" />
          <span className="font-bold text-white tracking-tight">NumOS</span>
        </Link>
        <nav className="hidden md:flex items-center gap-8 text-sm font-mono tracking-wider">
          <Link href="/manifesto" className="text-gray-400 hover:text-white transition-colors">Manifesto</Link>
          <Link href="/compare" className="text-gray-400 hover:text-[#ccff00] transition-colors">Compare</Link>
          <Link href="/roadmap" className="text-gray-400 hover:text-white transition-colors">Roadmap</Link>
          <Link href="/docs" className="text-gray-400 hover:text-white transition-colors">Docs</Link>
        </nav>
        <div className="flex items-center gap-4">
          <a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" rel="noopener noreferrer" className="hidden sm:inline-block border border-white/20 hover:border-[#ccff00] hover:text-[#ccff00] text-xs font-mono px-4 py-2 rounded-full transition-colors">
            GITHUB
          </a>
        </div>
      </div>
    </header>
  );
}
