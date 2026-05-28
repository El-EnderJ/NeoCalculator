import type { Metadata } from "next";
import Script from "next/script";
import "./globals.css";

import Navbar from "@/components/Navbar";
import Footer from "@/components/Footer";
import SponsorModal from "@/components/SponsorModal";

export const metadata: Metadata = {
  title: "NeoCalculator — NumOS | The €20 Open-Source Graphing Calculator",
  description: "NumOS: Open-source scientific OS running CAS symbolic algebra, real-time physics, and 17 apps on a $5 ESP32-S3. Disrupting the $150 calculator monopoly.",
  keywords: [
    "NeoCalculator",
    "NumOS",
    "Open-source graphing calculator",
    "CAS engine",
    "ESP32-S3",
    "Symbolic algebra",
    "Open hardware",
  ],
  metadataBase: new URL("https://neocalculator.tech"),
  openGraph: {
    type: "website",
    title: "NeoCalculator: The €20 Open-Source Graphing Calculator",
    description: "Scientific Mastery, Decolonized. Open-source, ESP32-based graphing calculator with a heavy CAS engine for €20. Join the revolution.",
    url: "https://neocalculator.tech",
    images: [
      {
        url: "https://neocalculator.tech/og-image.jpg",
        width: 1200,
        height: 630,
        alt: "NeoCalculator",
      },
    ],
    siteName: "NeoCalculator",
  },
  twitter: {
    card: "summary_large_image",
    title: "NeoCalculator: The €20 Open-Source Graphing Calculator",
    description: "Scientific Mastery, Decolonized. Open-source, ESP32-based graphing calculator with a heavy CAS engine for €20. Join the revolution.",
    images: ["https://neocalculator.tech/og-image.jpg"],
  },
  icons: {
    icon: "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🔢</text></svg>",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <head>
        <link rel="preconnect" href="https://fonts.googleapis.com" />
        <link rel="preconnect" href="https://fonts.gstatic.com" crossOrigin="" />
        <link
          href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800;900&family=JetBrains+Mono:wght@400;500;700&display=swap"
          rel="stylesheet"
        />
        <link
          rel="stylesheet"
          href="https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.css"
        />
        <Script
          defer
          src="https://cloud.umami.is/script.js"
          data-website-id="1a6202d4-439e-48e6-a031-e74f75f1e6e3"
        />
      </head>
      <body className="antialiased">
        <Navbar />
        {children}
        <Footer />
        <SponsorModal />
      </body>
    </html>
  );
}
