import type { Metadata } from "next";
import { Inter, JetBrains_Mono } from "next/font/google";
import "./globals.css";

const inter = Inter({ subsets: ["latin"], weight: ["400", "500", "700", "900"], variable: "--font-inter" });
const jetbrainsMono = JetBrains_Mono({ subsets: ["latin"], variable: "--font-jetbrains-mono" });

export const metadata: Metadata = {
  title: "NumOS | Open Source Scientific Calculator OS",
  description: "An affordable graphing calculator alternative starring a high-performance CAS engine and ESP32-S3 firmware. Disrupting the market with a 70% BoM reduction.",
  keywords: ["Open Source Scientific Calculator OS", "High-performance CAS engine", "ESP32-S3 calculator firmware", "Affordable graphing calculator alternative", "Symbolic Algebra System open hardware", "Pro-CAS", "DAG-based expressions", "Hardware Decolonization"],
  authors: [{ name: "NumOS Team", url: "https://neocalculator.tech" }],
  openGraph: {
    title: "NumOS | The Ultimate Open Source Scientific Calculator OS",
    description: "High-End Math, Zero-Cost Gatekeeping. Built on ESP32-S3.",
    type: "website",
    url: "https://neocalculator.tech",
  },
};

import Navbar from "@/components/Navbar";
import Footer from "@/components/Footer";
import { ScrollProgress } from "@/components/ScrollProgress";

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className={`${inter.variable} ${jetbrainsMono.variable} font-sans bg-[#0a0a0a] text-white antialiased selection:bg-[#ccff00] selection:text-black flex flex-col min-h-screen`}>
        <ScrollProgress />
        <Navbar />
        <div className="flex-grow pt-16">
            {children}
        </div>
        <Footer />
      </body>
    </html>
  );
}
