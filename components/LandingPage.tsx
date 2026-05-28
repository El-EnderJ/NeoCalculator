'use client';

import { useEffect, useRef, useState, type CSSProperties } from 'react';
import gsap from 'gsap';
import ScrollTrigger from 'gsap/ScrollTrigger';

type ParticleEdge = [number, number];

type Particle = {
  edge: ParticleEdge;
  t: number;
  speed: number;
  color: string;
  size: number;
  alpha: number;
};

type Node = {
  x: number;
  y: number;
  r: number;
  vx: number;
  vy: number;
  color: string;
};

const MATH_MORPHS = [
  { code: 'd/dx [ x³ + sin(x) ]', result: '3x² + cos(x)' },
  { code: '∫ x·eˣ dx', result: 'eˣ(x − 1) + C' },
  { code: '3x³ − 2x + 1 = 0', result: 'x ≈ −0.876, 0.503, ...' },
  { code: '∇²φ = −ρ/ε₀', result: 'CAS solved ✓' },
  { code: 'd/dx [ ln(sin(x)) ]', result: 'cos(x) / sin(x)' },
];

export default function LandingPage() {
  const rootRef = useRef<HTMLDivElement>(null);
  const submittedRef = useRef(false);
  const [waitlistSuccess, setWaitlistSuccess] = useState(false);
  const [prefersReducedMotion, setPrefersReducedMotion] = useState(false);

  useEffect(() => {
    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    const updatePreference = () => setPrefersReducedMotion(media.matches);
    updatePreference();

    if (media.addEventListener) {
      media.addEventListener('change', updatePreference);
      return () => media.removeEventListener('change', updatePreference);
    }

    media.addListener(updatePreference);
    return () => media.removeListener(updatePreference);
  }, []);

  useEffect(() => {
    if (prefersReducedMotion) return;

    const root = rootRef.current;
    if (!root) return;

    gsap.registerPlugin(ScrollTrigger);

    const cleanupFns: Array<() => void> = [];

    // 1) HERO CANVAS — Circuit Flow Particles
    const heroCanvas = root.querySelector('#heroCanvas') as HTMLCanvasElement | null;
    if (heroCanvas) {
      const ctx = heroCanvas.getContext('2d');
      if (ctx) {
        let width = 0;
        let height = 0;
        let particles: Particle[] = [];
        let nodes: Node[] = [];
        let edges: ParticleEdge[] = [];

        const LIME = '#AFFE00';
        const BLUE = '#00D1FF';
        const PARTICLE_COUNT = 80;
        const NODE_COUNT = 18;

        const resize = () => {
          width = heroCanvas.width = heroCanvas.offsetWidth;
          height = heroCanvas.height = heroCanvas.offsetHeight;
        };

        const createNodes = () => {
          nodes = [];
          for (let i = 0; i < NODE_COUNT; i += 1) {
            nodes.push({
              x: Math.random() * width,
              y: Math.random() * height,
              r: Math.random() * 2 + 1,
              vx: (Math.random() - 0.5) * 0.3,
              vy: (Math.random() - 0.5) * 0.3,
              color: Math.random() > 0.6 ? LIME : BLUE,
            });
          }
          edges = [];
          for (let i = 0; i < nodes.length; i += 1) {
            for (let j = i + 1; j < nodes.length; j += 1) {
              const dx = nodes[i].x - nodes[j].x;
              const dy = nodes[i].y - nodes[j].y;
              if (Math.sqrt(dx * dx + dy * dy) < width * 0.25) {
                edges.push([i, j]);
              }
            }
          }
        };

        const hexToRgba = (hex: string, alpha: number) => {
          const r = parseInt(hex.slice(1, 3), 16);
          const g = parseInt(hex.slice(3, 5), 16);
          const b = parseInt(hex.slice(5, 7), 16);
          return `rgba(${r},${g},${b},${alpha})`;
        };

        const makeParticle = (): Particle => {
          const edge = edges[Math.floor(Math.random() * edges.length)] ?? [0, 0];
          return {
            edge,
            t: Math.random(),
            speed: Math.random() * 0.003 + 0.001,
            color: Math.random() > 0.5 ? LIME : BLUE,
            size: Math.random() * 2 + 1,
            alpha: Math.random() * 0.8 + 0.2,
          };
        };

        const createParticles = () => {
          particles = [];
          for (let i = 0; i < PARTICLE_COUNT; i += 1) {
            particles.push(makeParticle());
          }
        };

        const draw = () => {
          ctx.clearRect(0, 0, width, height);

          // Move nodes
          nodes.forEach((node) => {
            node.x += node.vx;
            node.y += node.vy;
            if (node.x < 0 || node.x > width) node.vx *= -1;
            if (node.y < 0 || node.y > height) node.vy *= -1;
          });

          // Draw edges
          edges.forEach(([i, j]) => {
            const a = nodes[i];
            const b = nodes[j];
            const dx = b.x - a.x;
            const dy = b.y - a.y;
            const dist = Math.sqrt(dx * dx + dy * dy);
            const alpha = Math.max(0, 1 - dist / (width * 0.25));
            ctx.beginPath();
            ctx.moveTo(a.x, a.y);
            ctx.lineTo(b.x, b.y);
            ctx.strokeStyle = `rgba(175,254,0,${alpha * 0.08})`;
            ctx.lineWidth = 1;
            ctx.stroke();
          });

          // Draw nodes
          nodes.forEach((node) => {
            ctx.beginPath();
            ctx.arc(node.x, node.y, node.r, 0, Math.PI * 2);
            ctx.fillStyle = node.color;
            ctx.globalAlpha = 0.5;
            ctx.fill();
            ctx.globalAlpha = 1;
          });

          // Move & draw particles
          particles.forEach((particle, idx) => {
            if (!nodes[particle.edge[0]] || !nodes[particle.edge[1]]) return;
            const edge = particle.edge;
            const a = nodes[edge[0]];
            const b = nodes[edge[1]];
            particle.t += particle.speed;
            if (particle.t > 1) {
              particles[idx] = makeParticle();
              return;
            }
            const x = a.x + (b.x - a.x) * particle.t;
            const y = a.y + (b.y - a.y) * particle.t;

            ctx.beginPath();
            ctx.arc(x, y, particle.size, 0, Math.PI * 2);
            ctx.fillStyle = particle.color;
            ctx.globalAlpha = particle.alpha * (1 - particle.t * 0.5);
            ctx.fill();

            // Glow trail
            ctx.beginPath();
            ctx.arc(x, y, particle.size * 2.5, 0, Math.PI * 2);
            const grd = ctx.createRadialGradient(x, y, 0, x, y, particle.size * 2.5);
            grd.addColorStop(0, hexToRgba(particle.color, 0.3));
            grd.addColorStop(1, 'transparent');
            ctx.fillStyle = grd;
            ctx.globalAlpha = 0.4;
            ctx.fill();
            ctx.globalAlpha = 1;
          });
        };

        let heroRaf = 0;
        let heroActive = true;

        const loop = () => {
          if (!heroActive) return;
          draw();
          heroRaf = requestAnimationFrame(loop);
        };

        const handleResize = () => {
          resize();
          createNodes();
          createParticles();
        };

        window.addEventListener('resize', handleResize);
        cleanupFns.push(() => window.removeEventListener('resize', handleResize));
        cleanupFns.push(() => {
          heroActive = false;
          cancelAnimationFrame(heroRaf);
        });

        resize();
        createNodes();
        createParticles();
        loop();
      }
    }

    // 2) CTA CANVAS — Radial Pulse
    const ctaCanvas = root.querySelector('#ctaCanvas') as HTMLCanvasElement | null;
    if (ctaCanvas) {
      const ctx = ctaCanvas.getContext('2d');
      if (ctx) {
        let width = 0;
        let height = 0;
        let t = 0;

        const resize = () => {
          width = ctaCanvas.width = ctaCanvas.offsetWidth;
          height = ctaCanvas.height = ctaCanvas.offsetHeight;
        };

        const draw = () => {
          ctx.clearRect(0, 0, width, height);
          t += 0.008;
          const cx = width / 2;
          const cy = height / 2;
          const rings = 4;
          for (let i = 0; i < rings; i += 1) {
            const phase = t + (i / rings) * Math.PI * 2;
            const r = 120 + Math.sin(phase) * 40 + i * 60;
            const alpha = 0.06 - i * 0.012;
            ctx.beginPath();
            ctx.arc(cx, cy, r, 0, Math.PI * 2);
            ctx.strokeStyle = `rgba(175,254,0,${alpha})`;
            ctx.lineWidth = 1.5;
            ctx.stroke();
          }
        };

        let ctaRaf = 0;
        let ctaActive = true;
        const loop = () => {
          if (!ctaActive) return;
          draw();
          ctaRaf = requestAnimationFrame(loop);
        };

        const handleResize = () => {
          resize();
        };

        window.addEventListener('resize', handleResize);
        cleanupFns.push(() => window.removeEventListener('resize', handleResize));
        cleanupFns.push(() => {
          ctaActive = false;
          cancelAnimationFrame(ctaRaf);
        });

        resize();
        loop();
      }
    }

    // 3) GSAP ANIMATIONS
    const ctx = gsap.context(() => {
      const tl = gsap.timeline({ delay: 0.2 });

      tl.fromTo('#heroBadge',
        { opacity: 0, y: -20 },
        { opacity: 1, y: 0, duration: 0.7, ease: 'power2.out' }
      );

      tl.fromTo('.title-line',
        { opacity: 0, y: 50, skewX: -4 },
        { opacity: 1, y: 0, skewX: 0, duration: 0.8, ease: 'expo.out', stagger: 0.12 },
        '-=0.3'
      );

      tl.fromTo('#heroSub',
        { opacity: 0, y: 20 },
        { opacity: 1, y: 0, duration: 0.6, ease: 'power2.out' },
        '-=0.4'
      );

      tl.fromTo('#heroMath',
        { opacity: 0, scale: 0.95 },
        { opacity: 1, scale: 1, duration: 0.6, ease: 'back.out(1.7)' },
        '-=0.3'
      );

      tl.fromTo('.hero-actions .btn',
        { opacity: 0, y: 16 },
        { opacity: 1, y: 0, duration: 0.5, ease: 'power2.out', stagger: 0.1 },
        '-=0.2'
      );

      tl.fromTo('#heroVideo',
        { opacity: 0, y: 30 },
        { opacity: 1, y: 0, duration: 0.8, ease: 'power2.out' },
        '-=0.2'
      );

      tl.fromTo('#heroStats',
        { opacity: 0, y: 20 },
        { opacity: 1, y: 0, duration: 0.6, ease: 'power2.out' },
        '-=0.3'
      );

      const animateFadeIn = (selector: string, stagger = 0.12) => {
        gsap.fromTo(selector,
          { opacity: 0, y: 40 },
          {
            opacity: 1,
            y: 0,
            duration: 0.7,
            ease: 'power2.out',
            stagger,
            scrollTrigger: {
              trigger: selector,
              start: 'top 85%',
              once: true,
            },
          }
        );
      };

      animateFadeIn('.arch-layer', 0.15);
      animateFadeIn('.mem-card', 0.12);
      animateFadeIn('.tech-card', 0.1);
      animateFadeIn('.feat-card', 0.1);
      animateFadeIn('.hw-card', 0.12);
      animateFadeIn('.tl-item', 0.08);

      root.querySelectorAll('.section-title').forEach((el) => {
        gsap.fromTo(el,
          { opacity: 0, y: 32 },
          {
            opacity: 1,
            y: 0,
            duration: 0.8,
            ease: 'power2.out',
            scrollTrigger: { trigger: el, start: 'top 88%', once: true },
          }
        );
      });

      root.querySelectorAll('.section-label').forEach((el) => {
        gsap.fromTo(el,
          { opacity: 0, x: -20 },
          {
            opacity: 1,
            x: 0,
            duration: 0.5,
            ease: 'power2.out',
            scrollTrigger: { trigger: el, start: 'top 88%', once: true },
          }
        );
      });

      gsap.fromTo('.compare-table tbody tr',
        { opacity: 0, x: -20 },
        {
          opacity: 1,
          x: 0,
          duration: 0.5,
          ease: 'power2.out',
          stagger: 0.06,
          scrollTrigger: { trigger: '#moatTable', start: 'top 80%', once: true },
        }
      );

      gsap.fromTo('.moat-callout',
        { opacity: 0, y: 20 },
        {
          opacity: 1,
          y: 0,
          duration: 0.6,
          ease: 'power2.out',
          scrollTrigger: { trigger: '.moat-callout', start: 'top 88%', once: true },
        }
      );

      gsap.fromTo('.cta-content',
        { opacity: 0, y: 40 },
        {
          opacity: 1,
          y: 0,
          duration: 0.9,
          ease: 'power2.out',
          scrollTrigger: { trigger: '.cta', start: 'top 75%', once: true },
        }
      );

      gsap.fromTo('.arch-arrow',
        { opacity: 0, y: -10 },
        {
          opacity: 1,
          y: 0,
          duration: 0.4,
          ease: 'bounce.out',
          stagger: 0.2,
          scrollTrigger: { trigger: '.arch-diagram', start: 'top 80%', once: true },
        }
      );

      const bars = root.querySelectorAll('.mem-bar');
      if (bars.length) {
        ScrollTrigger.create({
          trigger: '.mem-grid',
          start: 'top 80%',
          once: true,
          onEnter: () => {
            bars.forEach((bar) => {
              const target = getComputedStyle(bar).getPropertyValue('--pct').trim();
              gsap.fromTo(bar,
                { width: '0%' },
                { width: target, duration: 1.4, ease: 'power2.out', delay: 0.2 }
              );
            });
          },
        });
      }

      // Timeline progress line animation
      const timeline = root.querySelector('.timeline');
      if (timeline) {
        ScrollTrigger.create({
          trigger: timeline,
          start: 'top 80%',
          once: true,
          onEnter: () => {
            gsap.from(timeline, {
              '--progress': '0%',
              duration: 1.8,
              ease: 'power2.inOut',
            });
          },
        });
      }

      // Active timeline pulse
      const activeCard = root.querySelector('.tl-card-active');
      if (activeCard) {
        gsap.to(activeCard, {
          boxShadow: '0 0 24px rgba(175,254,0,0.2)',
          repeat: -1,
          yoyo: true,
          duration: 2,
          ease: 'sine.inOut',
        });
      }
    }, root);

    cleanupFns.push(() => ctx.revert());

    // 4) Math morph cycling
    const codeEl = root.querySelector('#mathCode');
    const resultEl = root.querySelector('#mathResult');
    if (codeEl && resultEl) {
      let idx = 0;
      const intervalId = window.setInterval(() => {
        idx = (idx + 1) % MATH_MORPHS.length;
        const m = MATH_MORPHS[idx];

        gsap.to([codeEl, resultEl], {
          opacity: 0,
          y: -8,
          duration: 0.25,
          ease: 'power2.in',
          onComplete: () => {
            codeEl.textContent = m.code;
            resultEl.textContent = m.result;
            gsap.fromTo([codeEl, resultEl],
              { opacity: 0, y: 8 },
              { opacity: 1, y: 0, duration: 0.35, ease: 'power2.out' }
            );
          },
        });
      }, 3200);

      cleanupFns.push(() => window.clearInterval(intervalId));
    }

    // 5) Stat counters in hero
    const counterData = [
      { id: 'statBom', from: 0, to: 25, prefix: '$', suffix: '', unit: '' },
      { id: 'statRam', from: 0, to: 97, prefix: '', suffix: '', unit: 'KB' },
      { id: 'statApps', from: 0, to: 17, prefix: '', suffix: '', unit: '' },
      { id: 'statTests', from: 0, to: 85, prefix: '', suffix: '+', unit: '' },
      { id: 'statRoi', from: 0, to: 30, prefix: '', suffix: '×', unit: '' },
    ];

    const setStatContent = (
      el: Element,
      prefix: string,
      value: number,
      suffix: string,
      unit: string
    ) => {
      el.textContent = '';
      if (prefix) el.appendChild(document.createTextNode(prefix));
      el.appendChild(document.createTextNode(String(value)));
      if (unit) {
        const unitSpan = document.createElement('span');
        unitSpan.className = 'stat-unit';
        unitSpan.textContent = unit;
        el.appendChild(unitSpan);
      }
      if (suffix) el.appendChild(document.createTextNode(suffix));
    };

    const statsTimeoutId = window.setTimeout(() => {
      counterData.forEach((c) => {
        const el = root.querySelector(`#${c.id}`);
        if (!el) return;
        const obj = { n: c.from };
        gsap.to(obj, {
          n: c.to,
          duration: 1.8,
          ease: 'power2.out',
          delay: 0.3,
          onUpdate: () => {
            setStatContent(el, c.prefix, Math.round(obj.n), c.suffix, c.unit);
          },
          onComplete: () => {
            setStatContent(el, c.prefix, c.to, c.suffix, c.unit);
          },
        });
      });
    }, 1400);

    cleanupFns.push(() => window.clearTimeout(statsTimeoutId));

    // 6) Glitch title effect
    const accents = root.querySelectorAll('.title-accent');
    accents.forEach((el) => {
      el.setAttribute('data-text', el.textContent ?? '');
      el.classList.add('glitch-text');
    });
    cleanupFns.push(() => {
      accents.forEach((el) => {
        el.removeAttribute('data-text');
        el.classList.remove('glitch-text');
      });
    });

    // 7) Chip hover sparkle
    const chips = Array.from(root.querySelectorAll('.chip-highlight'));
    chips.forEach((chip) => {
      const onEnter = () => {
        gsap.to(chip, {
          boxShadow: '0 0 18px rgba(175,254,0,0.35)',
          duration: 0.25,
          ease: 'power2.out',
        });
      };
      const onLeave = () => {
        gsap.to(chip, {
          boxShadow: '0 0 0px rgba(175,254,0,0)',
          duration: 0.35,
          ease: 'power2.in',
        });
      };
      chip.addEventListener('mouseenter', onEnter);
      chip.addEventListener('mouseleave', onLeave);
      cleanupFns.push(() => {
        chip.removeEventListener('mouseenter', onEnter);
        chip.removeEventListener('mouseleave', onLeave);
      });
    });

    // 8) Feature card tilt (subtle 3D)
    const tiltCards = Array.from(root.querySelectorAll('.feat-card, .tech-card'));
    tiltCards.forEach((card) => {
      const onMove = (e: MouseEvent) => {
        const rect = card.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        const cx = rect.width / 2;
        const cy = rect.height / 2;
        const rotX = ((y - cy) / cy) * -4;
        const rotY = ((x - cx) / cx) * 4;
        gsap.to(card, {
          rotationX: rotX,
          rotationY: rotY,
          transformPerspective: 800,
          duration: 0.4,
          ease: 'power2.out',
        });
      };
      const onLeave = () => {
        gsap.to(card, {
          rotationX: 0,
          rotationY: 0,
          duration: 0.5,
          ease: 'power2.out',
        });
      };
      card.addEventListener('mousemove', onMove);
      card.addEventListener('mouseleave', onLeave);
      cleanupFns.push(() => {
        card.removeEventListener('mousemove', onMove);
        card.removeEventListener('mouseleave', onLeave);
      });
    });

    // 9) Compare table row highlight
    const rows = Array.from(root.querySelectorAll('.compare-table tbody tr'));
    rows.forEach((row) => {
      const onEnter = () => {
        gsap.to(row, {
          backgroundColor: 'rgba(255,255,255,0.025)',
          duration: 0.2,
          ease: 'power2.out',
        });
      };
      const onLeave = () => {
        gsap.to(row, {
          backgroundColor: 'transparent',
          duration: 0.3,
          ease: 'power2.out',
        });
      };
      row.addEventListener('mouseenter', onEnter);
      row.addEventListener('mouseleave', onLeave);
      cleanupFns.push(() => {
        row.removeEventListener('mouseenter', onEnter);
        row.removeEventListener('mouseleave', onLeave);
      });
    });

    return () => {
      cleanupFns.forEach((fn) => fn());
    };
  }, [prefersReducedMotion]);

  const handleWaitlistSubmit = () => {
    submittedRef.current = true;
  };

  const handleWaitlistIframeLoad = () => {
    if (submittedRef.current) {
      setWaitlistSuccess(true);
    }
  };

  return (
    <div ref={rootRef}>
      {/* HERO */}
      <section className="hero" id="hero">
        {!prefersReducedMotion && <canvas id="heroCanvas" />}
        <div className="hero-grid-overlay" />

        <div className="hero-content">
          <div className="hero-badge" id="heroBadge">
            <span className="badge-dot" />
            <a
              href="https://www.mercatus.org/emergent-ventures"
              target="_blank"
              rel="noopener noreferrer"
            >
              <strong>Backed by Emergent Ventures</strong>
            </a>
            (Mercatus Center, George Mason University)
          </div>

          <h1 className="hero-title" id="heroTitle">
            <span className="title-line">Scientific</span>
            <span className="title-line title-accent">Mastery,</span>
            <span className="title-line">Decolonized.</span>
          </h1>

          <p className="hero-sub" id="heroSub">
            NumOS runs a full <span className="hl-blue">Symbolic Math Engine</span>,
            real-time <span className="hl-blue">Navier-Stokes physics</span>, and
            <span className="hl-lime"> 17 university-grade apps</span>
            on a <strong>$5 ESP32-S3</strong> with <strong>97 KB RAM</strong>.
          </p>

          <div className="hero-math" id="heroMath">
            <div className="math-morph" id="mathMorph">
              <span className="math-code" id="mathCode">d/dx [ x³ + sin(x) ]</span>
              <span className="math-arrow">→</span>
              <span className="math-result" id="mathResult">3x² + cos(x)</span>
            </div>
            <div className="math-label">Live symbolic CAS on $5 silicon</div>
          </div>

          <div className="hero-actions">
            <a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" rel="noopener" className="btn btn-primary">
              <svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
                <path d="M12 0C5.37 0 0 5.37 0 12c0 5.31 3.435 9.795 8.205 11.385.6.105.825-.255.825-.57 0-.285-.015-1.23-.015-2.235-3.015.555-3.795-.735-4.035-1.41-.135-.345-.72-1.41-1.23-1.695-.42-.225-1.02-.78-.015-.795.945-.015 1.62.87 1.845 1.23 1.08 1.815 2.805 1.305 3.495.99.105-.78.42-1.305.765-1.605-2.67-.3-5.46-1.335-5.46-5.925 0-1.305.465-2.385 1.23-3.225-.12-.3-.54-1.53.12-3.18 0 0 1.005-.315 3.3 1.23.96-.27 1.98-.405 3-.405s2.04.135 3 .405c2.295-1.56 3.3-1.23 3.3-1.23.66 1.65.24 2.88.12 3.18.765.84 1.23 1.905 1.23 3.225 0 4.605-2.805 5.625-5.475 5.925.435.375.81 1.095.81 2.22 0 1.605-.015 2.895-.015 3.3 0 .315.225.69.825.57A12.02 12.02 0 0 0 24 12c0-6.63-5.37-12-12-12z" />
              </svg>
              Star on GitHub
            </a>
            <a href="#waitlist-form" className="btn btn-outline">Join Waitlist</a>
            <a href="#moat" className="btn btn-ghost">See The Moat ↓</a>
          </div>

          <div
            style={{
              margin: '0 auto 48px',
              maxWidth: '680px',
              width: '100%',
              borderRadius: '20px',
              overflow: 'hidden',
              border: '1px solid rgba(175,254,0,0.25)',
              boxShadow: '0 20px 60px rgba(0,0,0,0.45)',
              background: 'rgba(10,10,14,0.6)',
            }}
          >
            <img
              src="/frames/frame_090.webp"
              alt="NeoCalculator prototype render"
              loading="eager"
              fetchPriority="high"
              style={{ width: '100%', display: 'block' }}
            />
          </div>
        </div>

        <div className="hero-stats" id="heroStats">
          <div className="stat-item" data-stat="bom">
            <div className="stat-number"><span className="stat-val" id="statBom">$25</span></div>
            <div className="stat-label">BOM cost</div>
          </div>
          <div className="stat-divider" />
          <div className="stat-item" data-stat="ram">
            <div className="stat-number"><span className="stat-val" id="statRam">97<span className="stat-unit">KB</span></span></div>
            <div className="stat-label">Total RAM</div>
          </div>
          <div className="stat-divider" />
          <div className="stat-item" data-stat="apps">
            <div className="stat-number"><span className="stat-val" id="statApps">17</span></div>
            <div className="stat-label">Live apps</div>
          </div>
          <div className="stat-divider" />
          <div className="stat-item" data-stat="tests">
            <div className="stat-number"><span className="stat-val" id="statTests">85+</span></div>
            <div className="stat-label">CAS unit tests</div>
          </div>
          <div className="stat-divider" />
          <div className="stat-item" data-stat="roi">
            <div className="stat-number"><span className="stat-val" id="statRoi">30×</span></div>
            <div className="stat-label">Cost advantage</div>
          </div>
        </div>
      </section>

      {/* THE MOAT */}
      <section className="section moat" id="moat">
        <div className="container">
          <div className="section-label">THE MOAT</div>
          <h2 className="section-title">$150 Legacy Hardware<br /><span className="hl-lime">vs.</span> $5 Silicon.</h2>
          <p className="section-sub">The incumbent calculators haven't changed their core architecture in 30 years. We run on a processor that costs less than a coffee — and surpass them in symbolic computation.</p>

          <div className="table-wrapper" id="moatTable">
            <table className="compare-table">
              <thead>
                <tr>
                  <th className="col-feature">Capability</th>
                  <th className="col-numos winner">
                    <div className="th-logo">NumOS</div>
                    <div className="th-price price-lime">~$25</div>
                    <div className="th-badge">OPEN SOURCE</div>
                  </th>
                  <th>
                    <div className="th-logo">TI-84 Plus CE</div>
                    <div className="th-price">~$119</div>
                  </th>
                  <th>
                    <div className="th-logo">HP Prime G2</div>
                    <div className="th-price">~$179</div>
                  </th>
                  <th>
                    <div className="th-logo">NumWorks</div>
                    <div className="th-price">~$79</div>
                  </th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Symbolic CAS (Derivatives)</td>
                  <td className="winner"><span className="check">✓</span> Pro — 17 rules</td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="check dim">✓</span></td>
                  <td><span className="check dim">✓</span> SymPy</td>
                </tr>
                <tr>
                  <td>Symbolic Integration (Slagle)</td>
                  <td className="winner"><span className="check">✓</span> Slagle heuristic</td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="check dim">✓</span></td>
                  <td><span className="check dim">✓</span></td>
                </tr>
                <tr>
                  <td>Step-by-step Solutions</td>
                  <td className="winner"><span className="check">✓</span> Full steps</td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="check dim">✓</span></td>
                  <td><span className="cross">✗</span></td>
                </tr>
                <tr>
                  <td>Real-time Physics Simulation</td>
                  <td className="winner"><span className="check">✓</span> Verlet + N-S</td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="cross">✗</span></td>
                </tr>
                <tr>
                  <td>Particle Sandbox (30+ materials)</td>
                  <td className="winner"><span className="check">✓</span> Powder-Toy class</td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="cross">✗</span></td>
                </tr>
                <tr>
                  <td>Natural Display V.P.A.M.</td>
                  <td className="winner"><span className="check">✓</span></td>
                  <td><span className="check dim">✓</span></td>
                  <td><span className="check dim">✓</span></td>
                  <td><span className="check dim">✓</span></td>
                </tr>
                <tr>
                  <td>Open-Source (GPL v3)</td>
                  <td className="winner"><span className="check">✓</span></td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="check dim">✓</span></td>
                </tr>
                <tr>
                  <td>BigNum Arithmetic (Overflow-safe)</td>
                  <td className="winner"><span className="check">✓</span> CASInt/CASRational</td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="cross">✗</span></td>
                  <td><span className="cross">✗</span></td>
                </tr>
                <tr>
                  <td>MCU / Core</td>
                  <td className="winner">ESP32-S3 LX7 @ 240 MHz</td>
                  <td>Zilog eZ80</td>
                  <td>ARM Cortex-A7</td>
                  <td>STM32F730</td>
                </tr>
                <tr>
                  <td>Hardware Cost (BOM)</td>
                  <td className="winner price-lime">~$25</td>
                  <td>~$119</td>
                  <td>~$179</td>
                  <td>~$79</td>
                </tr>
              </tbody>
            </table>
          </div>

          <div className="moat-callout">
            <span className="callout-icon">⚡</span>
            <strong>NumOS already surpasses the TI-84 in CAS capability</strong> — at 8× lower cost.
            On track to match the NumWorks with WiFi connectivity and Python scripting.
          </div>
        </div>
      </section>

      {/* TECH STACK */}
      <section className="section tech" id="tech">
        <div className="container">
          <div className="section-label">TECH STACK — X-RAY</div>
          <h2 className="section-title">How we squeeze <span className="hl-lime">university-grade CAS</span><br />into 97 KB.</h2>

          <div className="arch-diagram" id="archDiagram">
            <div className="arch-layer" data-layer="0">
              <div className="arch-label">Applications (17 apps)</div>
              <div className="arch-chips">
                <div className="chip">Calculation</div>
                <div className="chip">Equations</div>
                <div className="chip">Calculus</div>
                <div className="chip">Grapher</div>
                <div className="chip">Statistics</div>
                <div className="chip">Bridge Designer</div>
                <div className="chip chip-highlight">Particle Lab</div>
                <div className="chip">Neural Lab</div>
                <div className="chip chip-more">+ 9 more</div>
              </div>
            </div>
            <div className="arch-arrow">▼</div>
            <div className="arch-layer" data-layer="1">
              <div className="arch-label">CAS Engine <span className="arch-tag">PSRAM</span></div>
              <div className="arch-chips">
                <div className="chip chip-highlight">SymExpr DAG (hash-consed)</div>
                <div className="chip chip-highlight">SymDiff — 17 rules</div>
                <div className="chip chip-highlight">SymIntegrate — Slagle</div>
                <div className="chip chip-highlight">OmniSolver</div>
                <div className="chip chip-highlight">BigNum (CASInt/Rational)</div>
                <div className="chip chip-highlight">8-pass Simplifier</div>
              </div>
            </div>
            <div className="arch-arrow">▼</div>
            <div className="arch-layer" data-layer="2">
              <div className="arch-label">Math Engine</div>
              <div className="arch-chips">
                <div className="chip">Tokenizer</div>
                <div className="chip">Shunting-Yard Parser</div>
                <div className="chip">RPN Evaluator</div>
                <div className="chip">Natural Display AST</div>
              </div>
            </div>
            <div className="arch-arrow">▼</div>
            <div className="arch-layer" data-layer="3">
              <div className="arch-label">Hardware Abstraction</div>
              <div className="arch-chips">
                <div className="chip">LVGL 9.x UI</div>
                <div className="chip">TFT_eSPI FSPI DMA</div>
                <div className="chip">KeyMatrix 5×10</div>
                <div className="chip">LittleFS</div>
              </div>
            </div>
          </div>

          <div className="mem-section">
            <h3 className="mem-title">Hybrid PSRAM / Internal RAM Architecture</h3>
            <div className="mem-grid">
              <div className="mem-card" id="memCard0">
                <div className="mem-icon">🧠</div>
                <div className="mem-name">Internal SRAM</div>
                <div className="mem-size">327 KB total</div>
                <div className="mem-bar-wrap">
                  <div className="mem-bar mem-bar-used" style={{ '--pct': '28.8%' } as CSSProperties}>
                    <span>28.8% used</span>
                  </div>
                </div>
                <div className="mem-detail">97 KB used · LVGL + DMA buffers + stack</div>
              </div>
              <div className="mem-card" id="memCard1">
                <div className="mem-icon">💎</div>
                <div className="mem-name">PSRAM OPI</div>
                <div className="mem-size">8 MB total</div>
                <div className="mem-bar-wrap">
                  <div className="mem-bar mem-bar-psram" style={{ '--pct': '2%' } as CSSProperties}>
                    <span>&lt;2% used</span>
                  </div>
                </div>
                <div className="mem-detail">CAS DAGs + step logs + particle grids</div>
              </div>
              <div className="mem-card" id="memCard2">
                <div className="mem-icon">⚡</div>
                <div className="mem-name">Flash</div>
                <div className="mem-size">16 MB total</div>
                <div className="mem-bar-wrap">
                  <div className="mem-bar mem-bar-flash" style={{ '--pct': '23.2%' } as CSSProperties}>
                    <span>23.2% used</span>
                  </div>
                </div>
                <div className="mem-detail">1.52 MB firmware · LittleFS variables</div>
              </div>
            </div>
          </div>

          <div className="tech-stack-grid">
            <div className="tech-card" id="tc0">
              <div className="tc-icon">🔧</div>
              <h4>ESP32-S3 LX7</h4>
              <p>Dual-core @ 240 MHz. Native USB. The same chip powering industrial IoT — at commodity price.</p>
            </div>
            <div className="tech-card" id="tc1">
              <div className="tc-icon">📐</div>
              <h4>CAS Engine</h4>
              <p>Powered by the Giac computer algebra system, with the initial backing and support of Bernard Parisse. Running smoothly on the ESP32-S3.</p>
            </div>
            <div className="tech-card" id="tc2">
              <div className="tc-icon">🌊</div>
              <h4>Real-Time Physics</h4>
              <p>Verlet integration bridge simulator. Navier-Stokes-inspired fluid dynamics. 30+ material particle sandbox — all at 60 Hz on 97 KB.</p>
            </div>
            <div className="tech-card" id="tc3">
              <div className="tc-icon">🖥️</div>
              <h4>LVGL 9.x UI</h4>
              <p>320×240 IPS colour display. Double DMA flush pipeline. Natural Display renders real fractions, radicals, and superscripts as on paper.</p>
            </div>
            <div className="tech-card" id="tc4">
              <div className="tc-icon">🧪</div>
              <h4>85+ Unit Tests</h4>
              <p>Comprehensive CAS test suite: rational arithmetic, polynomial factoring, symbolic diff/integrate, OmniSolver, step deduplication.</p>
            </div>
            <div className="tech-card" id="tc5">
              <div className="tc-icon">🔓</div>
              <h4>GPL v3 + CERN-OHL-S</h4>
              <p>Software under GPL v3. Hardware under CERN Open Hardware Licence. Fork it, build it, ship it — the ecosystem stays open forever.</p>
            </div>
          </div>
        </div>
      </section>

      {/* FEATURES */}
      <section className="section features" id="features">
        <div className="container">
          <div className="section-label">WHAT RUNS ON $5</div>
          <h2 className="section-title">17 Apps. One <span className="hl-lime">$5</span> Chip.</h2>

          <div className="features-grid">
            <div className="feat-card feat-large" id="feat0">
              <div className="feat-tag">CAS ENGINE</div>
              <h3>Symbolic Math</h3>
              <div className="feat-math">
                <div className="feat-eq">∫ x·eˣ dx = eˣ(x−1) + C</div>
                <div className="feat-eq">d/dx[sin(x²)] = 2x·cos(x²)</div>
                <div className="feat-eq">3x³−2x+1 = 0 → step-by-step</div>
              </div>
              <p>Powered by the robust Giac engine. Capable of limits, symbolic integration, differentiation, equation solving, and matrix operations directly on-device.</p>
            </div>

            <div className="feat-card" id="feat1">
              <div className="feat-tag">PHYSICS</div>
              <h3>Bridge Designer</h3>
              <p>Real-time Verlet physics. Stress analysis with live beam colour mapping (green→red). Wood / Steel / Cable materials. Truck and car load testing at 60 Hz.</p>
            </div>

            <div className="feat-card" id="feat2">
              <div className="feat-tag">SANDBOX</div>
              <h3>Particle Lab</h3>
              <p>30+ materials: Sand, Water, Lava, LN2, Wire, Iron, C4, Clone. Spark electronics with Joule heating. Phase transitions. Bresenham line tool. LittleFS save/load.</p>
            </div>

            <div className="feat-card" id="feat3">
              <div className="feat-tag">AI</div>
              <h3>Neural Lab</h3>
              <p>On-device neural net training: SGD+Momentum, L2 regularisation, 5 scenarios (XOR, Classifier, Sine, Spiral), bilinear upscale heatmap. Save/load via LittleFS.</p>
            </div>

            <div className="feat-card" id="feat4">
              <div className="feat-tag">GRAPHING</div>
              <h3>Function Grapher</h3>
              <p>y = f(x) real-time colour graphing. Zoom and pan. Value table. Complex expression support via the full Math Engine pipeline.</p>
            </div>

            <div className="feat-card" id="feat5">
              <div className="feat-tag">STATS</div>
              <h3>Statistics Suite</h3>
              <p>Statistics, Probability, Regression, Sequences apps. Full numeric pipeline backed by the Math Engine evaluator and variable store.</p>
            </div>
          </div>
        </div>
      </section>

      {/* HARDWARE */}
      <section className="section hardware" id="hardware">
        <div className="container">
          <div className="section-label">HARDWARE STATUS</div>
          <h2 className="section-title">From Breadboard<br />to <span className="hl-lime">4-Layer PCB.</span></h2>

          <div className="hw-status-grid">
            <div className="hw-card hw-active" id="hw0">
              <div className="hw-status-dot hw-dot-active" />
              <div className="hw-phase">PHASE ACTIVE</div>
              <h3>Input Bridge / HIL Testing</h3>
              <p>Hardware-In-the-Loop validation on breadboard prototype. Full NumOS firmware running. All 17 apps live. 5×10 key matrix wired (3 columns active). ILI9341 IPS 320×240 display at 10 MHz SPI. USB CDC console.</p>
              <div className="hw-specs">
                <div className="hw-spec"><span className="spec-k">MCU</span><span className="spec-v">ESP32-S3 N16R8 @ 240 MHz</span></div>
                <div className="hw-spec"><span className="spec-k">Display</span><span className="spec-v">ILI9341 IPS 3.2" · 320×240</span></div>
                <div className="hw-spec"><span className="spec-k">Keys</span><span className="spec-v">5×10 matrix · 15 wired</span></div>
                <div className="hw-spec"><span className="spec-k">Storage</span><span className="spec-v">16 MB Flash · 8 MB PSRAM OPI</span></div>
              </div>
            </div>

            <div className="hw-card hw-inprogress" id="hw1">
              <div className="hw-status-dot hw-dot-progress" />
              <div className="hw-phase">IN DESIGN</div>
              <h3>4-Layer PCB</h3>
              <p>Custom 4-layer PCB integrating ESP32-S3, TP4056 Li-Po charger, 5×10 mechanical keyboard matrix, display connector, and USB-C power delivery. KiCad schematic in active design.</p>
              <div className="hw-specs">
                <div className="hw-spec"><span className="spec-k">Layers</span><span className="spec-v">4 (Signal / GND / PWR / Signal)</span></div>
                <div className="hw-spec"><span className="spec-k">Form factor</span><span className="spec-v">NumWorks/TI-84 Plus CE size</span></div>
                <div className="hw-spec"><span className="spec-k">Charging</span><span className="spec-v">TP4056 Li-Po via USB-C</span></div>
                <div className="hw-spec"><span className="spec-k">Tool</span><span className="spec-v">KiCad</span></div>
              </div>
            </div>

            <div className="hw-card hw-planned" id="hw2">
              <div className="hw-status-dot hw-dot-planned" />
              <div className="hw-phase">IN DESIGN</div>
              <h3>3D-Printed Chassis</h3>
              <p>Custom ergonomic enclosure designed to fit the 4-layer PCB. Dimensional specifications locked. Manufacturing-ready design targeting FDM and SLA printing for prototype runs.</p>
              <div className="hw-specs">
                <div className="hw-spec"><span className="spec-k">Design tool</span><span className="spec-v">CAD (dimensions locked)</span></div>
                <div className="hw-spec"><span className="spec-k">Target</span><span className="spec-v">FDM + SLA prototypes</span></div>
                <div className="hw-spec"><span className="spec-k">BOM target</span><span className="spec-v">&lt;$25 total hardware</span></div>
              </div>
            </div>
          </div>

          <div className="hw-callout">
            <div className="hc-inner">
              <div className="hc-icon">🔬</div>
              <div className="hc-text">
                <strong>Every bug documented. Every fix published.</strong>
                <span>6 critical ESP32-S3 bring-up issues resolved, fully documented in <code>docs/HARDWARE.md</code>. The community can replicate this build today.</span>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* ROADMAP */}
      <section className="section roadmap" id="roadmap">
        <div className="container">
          <div className="section-label">ROADMAP</div>
          <h2 className="section-title">From Prototype to<br /><span className="hl-lime">Global Open Standard.</span></h2>

          <div className="timeline" id="timeline">
            <div className="tl-item tl-done" data-idx="0">
              <div className="tl-dot tl-dot-done" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">Feb 2026</div>
                <h4>First Light</h4>
                <p>First correct numerical calculation on serial terminal. Tokenizer → Parser → Evaluator pipeline live.</p>
              </div>
            </div>

            <div className="tl-item tl-done" data-idx="1">
              <div className="tl-dot tl-dot-done" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">Feb 2026</div>
                <h4>Natural Display</h4>
                <p>Real stacked fractions on TFT screen. V.P.A.M. visual AST with 2D cursor navigation.</p>
              </div>
            </div>

            <div className="tl-item tl-done" data-idx="2">
              <div className="tl-dot tl-dot-done" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">Feb 2026</div>
                <h4>Launcher 3.0</h4>
                <p>LVGL 9.x · animated splash screen · 3×N icon grid · SerialBridge for PC control.</p>
              </div>
            </div>

            <div className="tl-item tl-done" data-idx="3">
              <div className="tl-dot tl-dot-done" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">Feb 2026</div>
                <h4>CAS Engine</h4>
                <p>Full symbolic algebra: BigNum, DAG, SymDiff (17 rules), Slagle integrals, 8-pass simplifier, OmniSolver. 85+ unit tests.</p>
              </div>
            </div>

            <div className="tl-item tl-done" data-idx="4">
              <div className="tl-dot tl-dot-done" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">Mar–Apr 2026</div>
                <h4>17-App Platform</h4>
                <p>Bridge Designer (Verlet physics), Particle Lab (30+ materials), Neural Lab, Statistics, Probability, Regression, Sequences, Python — Phase 6 complete.</p>
              </div>
            </div>

            <div className="tl-item tl-active" data-idx="5">
              <div className="tl-dot tl-dot-active" />
              <div className="tl-line" />
              <div className="tl-card tl-card-active">
                <div className="tl-date">NOW</div>
                <h4>HIL Testing + PCB Design</h4>
                <p>Hardware-In-the-Loop validation. 4-layer PCB in KiCad. 3D chassis design. Input bridge firmware. All apps live on breadboard.</p>
              </div>
            </div>

            <div className="tl-item tl-planned" data-idx="6">
              <div className="tl-dot tl-dot-planned" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">Phase 7</div>
                <h4>Complete integration of Giac into the system</h4>
                <p>Full integration of the Giac computer algebra system into the existing architecture to work seamlessly with every app.</p>
              </div>
            </div>

            <div className="tl-item tl-planned" data-idx="7">
              <div className="tl-dot tl-dot-planned" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">Phase 8</div>
                <h4>Connectivity + Scripting</h4>
                <p>WiFi sync. Python REPL. USB file transfer. Battery charging via TP4056. Community app store.</p>
              </div>
            </div>

            <div className="tl-item tl-planned" data-idx="8">
              <div className="tl-dot tl-dot-planned" />
              <div className="tl-line" />
              <div className="tl-card">
                <div className="tl-date">V1.0</div>
                <h4>Global Open Hardware Release</h4>
                <p>Production-ready PCB. BOM &lt;$25. Full documentation for community manufacturing. The open-source TI killer — shipped.</p>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* WAITLIST */}
      <section className="section waitlist" id="waitlist">
        <div className="container">
          <div className="section-label">HARDWARE WAITLIST</div>
          <h2 className="section-title">Join the <span className="hl-lime">Hardware Waitlist</span></h2>
          <p className="section-sub">Get notified the exact day we launch our custom multi-layer PCB batch and open beta unit shipping.</p>

          <div className="waitlist-card">
            <iframe
              name="hidden_iframe"
              id="hidden_iframe"
              style={{ display: 'none' }}
              onLoad={handleWaitlistIframeLoad}
              title="Waitlist Hidden Iframe"
            />

            <form
              id="waitlist-form"
              action="https://docs.google.com/forms/u/0/d/e/1FAIpQLSfUGsT-RmTVXEdsH81NQQacNxyfCXgKxEXXilgURMApki_wiw/formResponse"
              method="POST"
              target="hidden_iframe"
              onSubmit={handleWaitlistSubmit}
              style={{ display: waitlistSuccess ? 'none' : undefined }}
            >
              <div className="wl-row">
                <div className="wl-field">
                  <label htmlFor="wl-name">Full Name</label>
                  <input type="text" id="wl-name" name="entry.815922704" placeholder="John Doe" required />
                </div>
                <div className="wl-field">
                  <label htmlFor="wl-role">Role / Position</label>
                  <input type="text" id="wl-role" name="entry.880764329" placeholder="Systems Engineer" required />
                </div>
              </div>
              <div className="wl-field">
                <label htmlFor="wl-email">Email Address</label>
                <input type="email" id="wl-email" name="entry.1495404027" placeholder="john@example.com" required />
              </div>
              <button type="submit" className="btn btn-primary btn-wl">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                  <path d="M22 2L11 13" />
                  <polygon points="22 2 15 22 11 13 2 9 22 2" />
                </svg>
                Join Waitlist
              </button>
              <div className="wl-privacy">We respect your data. No spam, just pure hardware engineering updates. Unsubscribe anytime.</div>
            </form>

            <div id="waitlist-success" style={{ display: waitlistSuccess ? 'block' : 'none' }}>
              <div className="success-icon">🚀</div>
              <h3>Welcome to the movement.</h3>
              <p>You're on the list.</p>
            </div>
          </div>
        </div>
      </section>

      {/* CTA */}
      <section className="section cta" id="cta">
        <div className="container">
          {prefersReducedMotion ? (
            <div
              aria-hidden="true"
              style={{
                position: 'absolute',
                inset: 0,
                background: 'radial-gradient(ellipse 60% 50% at 50% 50%, rgba(175,254,0,0.08), transparent)',
              }}
            />
          ) : (
            <canvas id="ctaCanvas" />
          )}
          <div className="cta-content">
            <div className="section-label">JOIN THE MISSION</div>
            <h2 className="cta-title">The calculator industry<br />hasn't been disrupted yet.</h2>
            <p className="cta-sub">Built by a 15-year-old systems architect from Spain. Invited to the <a href="https://www.1517fund.com/" target="_blank" rel="noopener noreferrer"><strong>1517 Fund</strong></a> builder community. EV Grantee. The code is live, the hardware works, and the monopoly is crumbling.</p>
            <div className="cta-actions">
              <a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" rel="noopener" className="btn btn-primary btn-xl">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
                  <path d="M12 0C5.37 0 0 5.37 0 12c0 5.31 3.435 9.795 8.205 11.385.6.105.825-.255.825-.57 0-.285-.015-1.23-.015-2.235-3.015.555-3.795-.735-4.035-1.41-.135-.345-.72-1.41-1.23-1.695-.42-.225-1.02-.78-.015-.795.945-.015 1.62.87 1.845 1.23 1.08 1.815 2.805 1.305 3.495.99.105-.78.42-1.305.765-1.605-2.67-.3-5.46-1.335-5.46-5.925 0-1.305.465-2.385 1.23-3.225-.12-.3-.54-1.53.12-3.18 0 0 1.005-.315 3.3 1.23.96-.27 1.98-.405 3-.405s2.04.135 3 .405c2.295-1.56 3.3-1.23 3.3-1.23.66 1.65.24 2.88.12 3.18.765.84 1.23 1.905 1.23 3.225 0 4.605-2.805 5.625-5.475 5.925.435.375.81 1.095.81 2.22 0 1.605-.015 2.895-.015 3.3 0 .315.225.69.825.57A12.02 12.02 0 0 0 24 12c0-6.63-5.37-12-12-12z" />
                </svg>
                Star on GitHub
              </a>
              <button type="button" className="btn btn-sponsor-cta btn-xl" data-sponsor-open>
                ♥ Support the Revolution
              </button>
              <a href="mailto:el.enderj2020@gmail.com" className="btn btn-outline btn-xl">✉ Email me</a>
              <a href="https://github.com/El-EnderJ/NeoCalculator/issues" target="_blank" rel="noopener" className="btn btn-outline btn-xl">Contribute</a>
            </div>
            <div className="cta-meta">
              <span>GPL v3 + CERN-OHL-S</span>
              <span className="meta-dot">·</span>
              <span>ESP32-S3 N16R8</span>
              <span className="meta-dot">·</span>
              <span>C++17</span>
              <span className="meta-dot">·</span>
              <span>LVGL 9.x</span>
              <span className="meta-dot">·</span>
              <span>neocalculator.tech</span>
            </div>
          </div>
        </div>
      </section>
    </div>
  );
}
