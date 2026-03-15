'use client';

import { useEffect, useRef } from 'react';
import gsap from 'gsap';
import ScrollTrigger from 'gsap/ScrollTrigger';

// Register the GSAP plugin exclusively on the client side
if (typeof window !== 'undefined') {
  gsap.registerPlugin(ScrollTrigger);
}

export default function HardwareScroll() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  
  useEffect(() => {
    const canvas = canvasRef.current;
    const container = containerRef.current;
    if (!canvas || !container) return;
    
    const context = canvas.getContext('2d');
    if (!context) return;

    // Fixed High-Res Canvas Size for Drawing
    canvas.width = 1920;
    canvas.height = 1080;

    const frameCount = 120;
    // Assuming frames are in public/frames/frame_001.webp 
    // They will return 404 until you generate/add them to the repo
    const currentFrame = (index: number) => 
      `/frames/frame_${(index + 1).toString().padStart(3, '0')}.webp`;

    const images: HTMLImageElement[] = [];
    const obj = { frame: 0 };

    for (let i = 0; i < frameCount; i++) {
        const img = new Image();
        img.src = currentFrame(i); // Preloads automatically by browsers efficiently
        images.push(img);
    }

    const render = () => {
      context.clearRect(0, 0, canvas.width, canvas.height);
      if (images[obj.frame] && images[obj.frame].complete) {
        // Draw the image, centered and scaled to cover
        const img = images[obj.frame];
        const hRatio = canvas.width / img.width;
        const vRatio = canvas.height / img.height;
        const ratio = Math.max(hRatio, vRatio);
        const centerShift_x = (canvas.width - img.width * ratio) / 2;
        const centerShift_y = (canvas.height - img.height * ratio) / 2;
        context.drawImage(img, 0, 0, img.width, img.height,
           centerShift_x, centerShift_y, img.width * ratio, img.height * ratio);
      }
    };

    // Make sure first image is drawn when loaded
    images[0].onload = render;

    const tl = gsap.timeline({
      scrollTrigger: {
        trigger: container,
        start: 'top top',
        end: '+=4000', // How long the scroll takes (4000px duration)
        scrub: 1, // Smooth scrubbing, takes 1 second to "catch up" to the scrollbar
        pin: true,
      }
    });

    tl.to(obj, {
      frame: frameCount - 1,
      snap: 'frame',
      ease: 'none',
      onUpdate: () => {
        requestAnimationFrame(render);
      }
    });

    // Cleanup process
    return () => {
      ScrollTrigger.getAll().forEach(t => t.kill());
    };
  }, []);

  return (
    <section ref={containerRef} className="h-screen w-full bg-[#0a0a0a] relative overflow-hidden">
      <div className="absolute top-12 w-full text-center z-20 pointer-events-none px-4">
        <h2 className="text-2xl md:text-5xl font-black text-white mix-blend-difference uppercase tracking-widest font-sans drop-shadow-md">
          Assemble the Future
        </h2>
        <p className="text-gray-400 font-mono mt-2 mix-blend-difference text-sm md:text-base">
          Scroll to deconstruct the architecture.
        </p>
      </div>
      
      <canvas 
        ref={canvasRef} 
        className="w-full h-full object-cover opacity-80"
      />
      
      {/* Vignette effect to transition smoothly into the next dark section */}
      <div className="absolute inset-0 bg-gradient-to-t from-[#0a0a0a] via-transparent to-transparent z-10 pointer-events-none"></div>
    </section>
  );
}
