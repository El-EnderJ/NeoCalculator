'use client';

import { useEffect, useState } from 'react';

export default function Navbar() {
  const [isOpen, setIsOpen] = useState(false);
  const [isScrolled, setIsScrolled] = useState(false);

  useEffect(() => {
    const onScroll = () => {
      setIsScrolled(window.scrollY > 40);
    };

    onScroll();
    window.addEventListener('scroll', onScroll, { passive: true });
    return () => window.removeEventListener('scroll', onScroll);
  }, []);

  const handleLinkClick = () => {
    setIsOpen(false);
  };

  return (
    <nav className={`nav${isScrolled ? ' scrolled' : ''}`} id="nav">
      <div className="nav-inner">
        <a href="#" className="nav-logo" onClick={handleLinkClick}>
          <span className="logo-neo">Neo</span><span className="logo-calc">Calculator</span>
          <span className="logo-badge">NumOS</span>
        </a>
        <ul className={`nav-links${isOpen ? ' open' : ''}`}>
          <li><a href="#moat" onClick={handleLinkClick}>The Moat</a></li>
          <li><a href="#tech" onClick={handleLinkClick}>Tech Stack</a></li>
          <li><a href="#hardware" onClick={handleLinkClick}>Hardware</a></li>
          <li><a href="#roadmap" onClick={handleLinkClick}>Roadmap</a></li>
          <li><a href="https://github.com/El-EnderJ/NeoCalculator#system-architecture" target="_blank" rel="noopener" onClick={handleLinkClick}>Docs / Architecture ↗</a></li>
          <li><a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" rel="noopener" className="btn-nav" onClick={handleLinkClick}>GitHub ↗</a></li>
          <li><a href="mailto:el.enderj2020@gmail.com" className="btn-nav btn-outline" onClick={handleLinkClick}>✉ Email me</a></li>
          <li><a href="https://ko-fi.com/enderdesigns" target="_blank" rel="noopener" className="btn-nav btn-kofi" onClick={handleLinkClick}>☕ Support Me</a></li>
          <li>
            <button type="button" className="btn-nav btn-sponsor" data-sponsor-open onClick={() => setIsOpen(false)}>
              ♥ Sponsor
            </button>
          </li>
        </ul>
        <button
          type="button"
          className="nav-burger"
          aria-label="Menu"
          id="navBurger"
          onClick={() => setIsOpen((prev) => !prev)}
        >
          <span></span><span></span><span></span>
        </button>
      </div>
    </nav>
  );
}
