'use client';

import { useEffect, useRef } from 'react';

export default function HeroCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationFrameId: number;
    // Particle interface
    type Particle = { x: number, y: number, vx: number, vy: number, size: number };
    let particles: Particle[] = [];
    const mouse = { x: -1000, y: -1000 };

    const resize = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
      initParticles();
    };

    const initParticles = () => {
      particles = [];
      const numParticles = Math.floor((canvas.width * canvas.height) / 15000); // Dynamic amount based on screen volume
      for (let i = 0; i < numParticles; i++) {
        particles.push({
          x: Math.random() * canvas.width,
          y: Math.random() * canvas.height,
          vx: (Math.random() - 0.5) * 0.5,
          vy: (Math.random() - 0.5) * 0.5,
          size: Math.random() * 2 + 0.5
        });
      }
    };

    const draw = () => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.fillStyle = 'rgba(204, 255, 0, 0.5)'; // Cyber Lime particles
      
      particles.forEach(p => {
        p.x += p.vx;
        p.y += p.vy;

        // Bounce horizontally and vertically
        if (p.x < 0 || p.x > canvas.width) p.vx *= -1;
        if (p.y < 0 || p.y > canvas.height) p.vy *= -1;

        // Mouse repelling interaction
        const dx = mouse.x - p.x;
        const dy = mouse.y - p.y;
        const distance = Math.sqrt(dx * dx + dy * dy);
        
        if (distance < 120) {
          p.x -= dx * 0.05;
          p.y -= dy * 0.05;
        }

        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
        ctx.fill();
      });

      animationFrameId = requestAnimationFrame(draw);
    };

    window.addEventListener('resize', resize);
    window.addEventListener('mousemove', (e) => {
      mouse.x = e.clientX;
      mouse.y = e.clientY;
    });

    resize();
    draw();

    return () => {
      window.removeEventListener('resize', resize);
      cancelAnimationFrame(animationFrameId);
    };
  }, []);

  return (
    <div className="relative h-screen w-full flex items-center justify-center overflow-hidden bg-[#0a0a0a]">
      <canvas ref={canvasRef} className="absolute inset-0 z-0 opacity-40"></canvas>
      <div className="relative z-10 text-center pointer-events-none px-4 flex flex-col items-center">
        <div className="inline-block border border-white/20 text-white/80 text-xs font-mono px-4 py-2 rounded-full mb-8 uppercase tracking-[0.2em] backdrop-blur-md bg-white/5">
          NeoCalculator &times; NumOS
        </div>
        <h1 className="text-6xl md:text-8xl lg:text-[7rem] font-black tracking-tighter mb-4 text-glow-lime drop-shadow-2xl leading-[0.9]">
          NUMOS<br/>
          <span className="text-white">REDEFINED</span>
        </h1>
        <p className="font-mono text-gray-400 max-w-2xl mx-auto text-lg md:text-xl mt-6 px-4">
          A <span className="text-white">$15</span> open-source scientific handheld disrupting a <span className="text-white">$150</span> monopoly.
        </p>

        <div className="mt-12 flex flex-col sm:flex-row gap-4 justify-center items-center w-full px-4">
          <a href="/manifesto" className="bg-[#ccff00] text-black font-black py-4 px-8 md:px-10 rounded-lg hover:bg-white transition-colors duration-300 w-full sm:w-auto text-center uppercase tracking-[0.1em] shadow-[0_0_20px_rgba(204,255,0,0.3)]">
            Read the Manifesto
          </a>
          <a href="/compare" className="border border-white/20 bg-black/50 backdrop-blur-md text-white font-bold py-4 px-8 md:px-10 rounded-lg hover:border-white/60 hover:bg-white/10 transition-all duration-300 w-full sm:w-auto text-center uppercase tracking-[0.1em]">
            View Specs
          </a>
        </div>
      </div>
    </div>
  );
}
