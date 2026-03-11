/**
 * NumOS / NeoCalculator — Landing Page JavaScript
 * Vite + Vanilla JS + GSAP (CDN)
 */

/* ────────────────────────────────────────────
   0. GSAP SETUP
──────────────────────────────────────────── */
gsap.registerPlugin(ScrollTrigger);

/* ────────────────────────────────────────────
   1. HERO CANVAS — Circuit Flow Particles
──────────────────────────────────────────── */
(function initHeroCanvas() {
  const canvas = document.getElementById('heroCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');

  let W, H, particles, nodes, edges;
  const LIME  = '#AFFE00';
  const BLUE  = '#00D1FF';
  const DIM   = 'rgba(175,254,0,0.15)';
  const PARTICLE_COUNT = 80;
  const NODE_COUNT     = 18;

  function resize() {
    W = canvas.width  = canvas.offsetWidth;
    H = canvas.height = canvas.offsetHeight;
  }

  function createNodes() {
    nodes = [];
    for (let i = 0; i < NODE_COUNT; i++) {
      nodes.push({
        x: Math.random() * W,
        y: Math.random() * H,
        r: Math.random() * 2 + 1,
        vx: (Math.random() - 0.5) * 0.3,
        vy: (Math.random() - 0.5) * 0.3,
        color: Math.random() > 0.6 ? LIME : BLUE,
      });
    }
    edges = [];
    for (let i = 0; i < nodes.length; i++) {
      for (let j = i + 1; j < nodes.length; j++) {
        const dx = nodes[i].x - nodes[j].x;
        const dy = nodes[i].y - nodes[j].y;
        if (Math.sqrt(dx * dx + dy * dy) < W * 0.25) {
          edges.push([i, j]);
        }
      }
    }
  }

  function createParticles() {
    particles = [];
    for (let i = 0; i < PARTICLE_COUNT; i++) {
      particles.push(makeParticle());
    }
  }

  // Convert hex color to rgba string
  function hexToRgba(hex, alpha) {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    return `rgba(${r},${g},${b},${alpha})`;
  }

  function makeParticle() {
    const edge = edges[Math.floor(Math.random() * edges.length)];
    return {
      edge,
      t: Math.random(),
      speed: Math.random() * 0.003 + 0.001,
      color: Math.random() > 0.5 ? LIME : BLUE,
      size: Math.random() * 2 + 1,
      alpha: Math.random() * 0.8 + 0.2,
    };
  }

  function draw() {
    ctx.clearRect(0, 0, W, H);

    // Move nodes
    nodes.forEach(n => {
      n.x += n.vx;
      n.y += n.vy;
      if (n.x < 0 || n.x > W) n.vx *= -1;
      if (n.y < 0 || n.y > H) n.vy *= -1;
    });

    // Draw edges
    edges.forEach(([i, j]) => {
      const a = nodes[i], b = nodes[j];
      const dx = b.x - a.x, dy = b.y - a.y;
      const dist = Math.sqrt(dx * dx + dy * dy);
      const alpha = Math.max(0, 1 - dist / (W * 0.25));
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(b.x, b.y);
      ctx.strokeStyle = `rgba(175,254,0,${alpha * 0.08})`;
      ctx.lineWidth = 1;
      ctx.stroke();
    });

    // Draw nodes
    nodes.forEach(n => {
      ctx.beginPath();
      ctx.arc(n.x, n.y, n.r, 0, Math.PI * 2);
      ctx.fillStyle = n.color;
      ctx.globalAlpha = 0.5;
      ctx.fill();
      ctx.globalAlpha = 1;
    });

    // Move & draw particles
    particles.forEach((p, idx) => {
      if (!nodes[p.edge[0]] || !nodes[p.edge[1]]) return;
      const edge = p.edge;
      const a = nodes[edge[0]], b = nodes[edge[1]];
      p.t += p.speed;
      if (p.t > 1) {
        particles[idx] = makeParticle();
        return;
      }
      const x = a.x + (b.x - a.x) * p.t;
      const y = a.y + (b.y - a.y) * p.t;

      ctx.beginPath();
      ctx.arc(x, y, p.size, 0, Math.PI * 2);
      ctx.fillStyle = p.color;
      ctx.globalAlpha = p.alpha * (1 - p.t * 0.5);
      ctx.fill();

      // Glow trail
      ctx.beginPath();
      ctx.arc(x, y, p.size * 2.5, 0, Math.PI * 2);
      const grd = ctx.createRadialGradient(x, y, 0, x, y, p.size * 2.5);
      grd.addColorStop(0, hexToRgba(p.color, 0.3));
      grd.addColorStop(1, 'transparent');
      ctx.fillStyle = grd;
      ctx.globalAlpha = 0.4;
      ctx.fill();
      ctx.globalAlpha = 1;
    });
  }

  function loop() {
    draw();
    requestAnimationFrame(loop);
  }

  window.addEventListener('resize', () => {
    resize();
    createNodes();
    createParticles();
  });

  resize();
  createNodes();
  createParticles();
  loop();
})();

/* ────────────────────────────────────────────
   2. CTA CANVAS — Radial Pulse
──────────────────────────────────────────── */
(function initCtaCanvas() {
  const canvas = document.getElementById('ctaCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  let W, H, t = 0;

  function resize() {
    W = canvas.width  = canvas.offsetWidth;
    H = canvas.height = canvas.offsetHeight;
  }

  function draw() {
    ctx.clearRect(0, 0, W, H);
    t += 0.008;
    const cx = W / 2, cy = H / 2;
    const rings = 4;
    for (let i = 0; i < rings; i++) {
      const phase = t + (i / rings) * Math.PI * 2;
      const r = 120 + Math.sin(phase) * 40 + i * 60;
      const alpha = 0.06 - i * 0.012;
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, Math.PI * 2);
      ctx.strokeStyle = `rgba(175,254,0,${alpha})`;
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }
  }

  function loop() {
    draw();
    requestAnimationFrame(loop);
  }

  window.addEventListener('resize', resize);
  resize();
  loop();
})();

/* ────────────────────────────────────────────
   3. NAV SCROLL BEHAVIOUR
──────────────────────────────────────────── */
(function initNav() {
  const nav = document.getElementById('nav');
  const burger = document.getElementById('navBurger');
  const links = document.querySelector('.nav-links');

  window.addEventListener('scroll', () => {
    nav.classList.toggle('scrolled', window.scrollY > 40);
  }, { passive: true });

  burger && burger.addEventListener('click', () => {
    links.classList.toggle('open');
  });

  // Close on link click
  links && links.querySelectorAll('a').forEach(a => {
    a.addEventListener('click', () => links.classList.remove('open'));
  });
})();

/* ────────────────────────────────────────────
   4. HERO GSAP ANIMATIONS
──────────────────────────────────────────── */
(function initHeroAnimations() {
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
})();

/* ────────────────────────────────────────────
   5. MATH MORPH CYCLING
──────────────────────────────────────────── */
(function initMathMorph() {
  const morphs = [
    { code: 'd/dx [ x³ + sin(x) ]',    result: '3x² + cos(x)' },
    { code: '∫ x·eˣ dx',               result: 'eˣ(x − 1) + C' },
    { code: '3x³ − 2x + 1 = 0',        result: 'x ≈ −0.876, 0.503, ...' },
    { code: '∇²φ = −ρ/ε₀',            result: 'CAS solved ✓' },
    { code: 'd/dx [ ln(sin(x)) ]',      result: 'cos(x) / sin(x)' },
  ];
  let idx = 0;
  const codeEl   = document.getElementById('mathCode');
  const resultEl = document.getElementById('mathResult');
  if (!codeEl || !resultEl) return;

  function cycle() {
    idx = (idx + 1) % morphs.length;
    const m = morphs[idx];

    gsap.to([codeEl, resultEl], {
      opacity: 0,
      y: -8,
      duration: 0.25,
      ease: 'power2.in',
      onComplete: () => {
        codeEl.textContent   = m.code;
        resultEl.textContent = m.result;
        gsap.fromTo([codeEl, resultEl],
          { opacity: 0, y: 8 },
          { opacity: 1, y: 0, duration: 0.35, ease: 'power2.out' }
        );
      }
    });
  }

  setInterval(cycle, 3200);
})();

/* ────────────────────────────────────────────
   6. SCROLL TRIGGER ANIMATIONS
──────────────────────────────────────────── */
(function initScrollAnimations() {

  // Generic fade-in cards
  function animateFadeIn(selector, stagger = 0.12) {
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
        }
      }
    );
  }

  animateFadeIn('.arch-layer', 0.15);
  animateFadeIn('.mem-card', 0.12);
  animateFadeIn('.tech-card', 0.1);
  animateFadeIn('.feat-card', 0.1);
  animateFadeIn('.hw-card', 0.12);
  animateFadeIn('.tl-item', 0.08);

  // Section titles
  document.querySelectorAll('.section-title').forEach(el => {
    gsap.fromTo(el,
      { opacity: 0, y: 32 },
      {
        opacity: 1, y: 0, duration: 0.8, ease: 'power2.out',
        scrollTrigger: { trigger: el, start: 'top 88%', once: true }
      }
    );
  });

  // Section labels
  document.querySelectorAll('.section-label').forEach(el => {
    gsap.fromTo(el,
      { opacity: 0, x: -20 },
      {
        opacity: 1, x: 0, duration: 0.5, ease: 'power2.out',
        scrollTrigger: { trigger: el, start: 'top 88%', once: true }
      }
    );
  });

  // Table rows stagger
  gsap.fromTo('.compare-table tbody tr',
    { opacity: 0, x: -20 },
    {
      opacity: 1, x: 0, duration: 0.5, ease: 'power2.out', stagger: 0.06,
      scrollTrigger: { trigger: '#moatTable', start: 'top 80%', once: true }
    }
  );

  // Moat callout
  gsap.fromTo('.moat-callout',
    { opacity: 0, y: 20 },
    {
      opacity: 1, y: 0, duration: 0.6, ease: 'power2.out',
      scrollTrigger: { trigger: '.moat-callout', start: 'top 88%', once: true }
    }
  );

  // CTA
  gsap.fromTo('.cta-content',
    { opacity: 0, y: 40 },
    {
      opacity: 1, y: 0, duration: 0.9, ease: 'power2.out',
      scrollTrigger: { trigger: '.cta', start: 'top 75%', once: true }
    }
  );

  // Arch diagram arrows
  gsap.fromTo('.arch-arrow',
    { opacity: 0, y: -10 },
    {
      opacity: 1, y: 0, duration: 0.4, ease: 'bounce.out', stagger: 0.2,
      scrollTrigger: { trigger: '.arch-diagram', start: 'top 80%', once: true }
    }
  );
})();

/* ────────────────────────────────────────────
   7. MEMORY BAR COUNTER ANIMATION
──────────────────────────────────────────── */
(function initMemBars() {
  const bars = document.querySelectorAll('.mem-bar');
  if (!bars.length) return;

  ScrollTrigger.create({
    trigger: '.mem-grid',
    start: 'top 80%',
    once: true,
    onEnter: () => {
      bars.forEach(bar => {
        const target = getComputedStyle(bar).getPropertyValue('--pct').trim();
        const pct = parseFloat(target);
        gsap.fromTo(bar,
          { width: '0%' },
          { width: target, duration: 1.4, ease: 'power2.out', delay: 0.2 }
        );
      });
    }
  });
})();

/* ────────────────────────────────────────────
   8. STAT COUNTERS IN HERO
──────────────────────────────────────────── */
(function initStatCounters() {
  // Each entry: { id, from, to, prefix, suffix, unit }
  const counterData = [
    { id: 'statBom',   from: 0,  to: 5,  prefix: '$', suffix: '',   unit: '' },
    { id: 'statRam',   from: 0,  to: 97, prefix: '',  suffix: '',   unit: 'KB' },
    { id: 'statApps',  from: 0,  to: 17, prefix: '',  suffix: '',   unit: '' },
    { id: 'statTests', from: 0,  to: 85, prefix: '',  suffix: '+',  unit: '' },
    { id: 'statRoi',   from: 0,  to: 30, prefix: '',  suffix: '×',  unit: '' },
  ];

  // Helper: safely set stat element content using DOM, not innerHTML
  function setStatContent(el, prefix, value, suffix, unit) {
    el.textContent = '';
    if (prefix) el.appendChild(document.createTextNode(prefix));
    el.appendChild(document.createTextNode(value));
    if (unit) {
      const unitSpan = document.createElement('span');
      unitSpan.className = 'stat-unit';
      unitSpan.textContent = unit;
      el.appendChild(unitSpan);
    }
    if (suffix) el.appendChild(document.createTextNode(suffix));
  }

  setTimeout(() => {
    counterData.forEach(c => {
      const el = document.getElementById(c.id);
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
        }
      });
    });
  }, 1400);
})();

/* ────────────────────────────────────────────
   9. GLITCH TITLE EFFECT
──────────────────────────────────────────── */
(function initGlitch() {
  const accents = document.querySelectorAll('.title-accent');
  accents.forEach(el => {
    el.setAttribute('data-text', el.textContent);
    el.classList.add('glitch-text');
  });
})();

/* ────────────────────────────────────────────
   10. CHIP HOVER SPARKLE
──────────────────────────────────────────── */
(function initChipEffects() {
  document.querySelectorAll('.chip-highlight').forEach(chip => {
    chip.addEventListener('mouseenter', () => {
      gsap.to(chip, {
        boxShadow: '0 0 18px rgba(175,254,0,0.35)',
        duration: 0.25,
        ease: 'power2.out'
      });
    });
    chip.addEventListener('mouseleave', () => {
      gsap.to(chip, {
        boxShadow: '0 0 0px rgba(175,254,0,0)',
        duration: 0.35,
        ease: 'power2.in'
      });
    });
  });
})();

/* ────────────────────────────────────────────
   11. TIMELINE PROGRESS LINE ANIMATION
──────────────────────────────────────────── */
(function initTimelineAnim() {
  const timeline = document.querySelector('.timeline');
  if (!timeline) return;

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
    }
  });
})();

/* ────────────────────────────────────────────
   12. ACTIVE TIMELINE PULSE
──────────────────────────────────────────── */
(function initActiveCardPulse() {
  const activeCard = document.querySelector('.tl-card-active');
  if (!activeCard) return;
  gsap.to(activeCard, {
    boxShadow: '0 0 24px rgba(175,254,0,0.2)',
    repeat: -1,
    yoyo: true,
    duration: 2,
    ease: 'sine.inOut',
  });
})();

/* ────────────────────────────────────────────
   13. FEATURE CARD TILT (subtle 3D)
──────────────────────────────────────────── */
(function initCardTilt() {
  document.querySelectorAll('.feat-card, .tech-card').forEach(card => {
    card.addEventListener('mousemove', e => {
      const rect = card.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      const cx = rect.width  / 2;
      const cy = rect.height / 2;
      const rotX = ((y - cy) / cy) * -4;
      const rotY = ((x - cx) / cx) *  4;
      gsap.to(card, {
        rotationX: rotX,
        rotationY: rotY,
        transformPerspective: 800,
        duration: 0.4,
        ease: 'power2.out',
      });
    });
    card.addEventListener('mouseleave', () => {
      gsap.to(card, {
        rotationX: 0,
        rotationY: 0,
        duration: 0.5,
        ease: 'power2.out',
      });
    });
  });
})();

/* ────────────────────────────────────────────
   14. COMPARE TABLE ROW HIGHLIGHT
──────────────────────────────────────────── */
(function initTableHighlight() {
  document.querySelectorAll('.compare-table tbody tr').forEach(row => {
    row.addEventListener('mouseenter', () => {
      gsap.to(row, {
        backgroundColor: 'rgba(255,255,255,0.025)',
        duration: 0.2,
        ease: 'power2.out',
      });
    });
    row.addEventListener('mouseleave', () => {
      gsap.to(row, {
        backgroundColor: 'transparent',
        duration: 0.3,
        ease: 'power2.out',
      });
    });
  });
})();
