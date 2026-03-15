import type { Metadata } from "next";
import { Inter, JetBrains_Mono } from "next/font/google";
import "./globals.css";

const inter = Inter({ subsets: ["latin"], variable: "--font-inter" });
const jetbrainsMono = JetBrains_Mono({ subsets: ["latin"], variable: "--font-jetbrains-mono" });

export const metadata: Metadata = {
  title: "NumOS | Open Source CAS Calculator",
  description: "A $15 open-source scientific calculator that outperforms $180 commercial models. Featuring an ESP32-S3 and Pro-CAS engine.",
  keywords: ["Open Source CAS", "ESP32-S3 Calculator", "Low-cost Scientific Computing", "NumOS", "Hardware"],
  authors: [{ name: "NumOS Team" }],
  openGraph: {
    title: "NumOS | Open Source CAS Calculator",
    description: "High-End Math, Zero-Cost Gatekeeping.",
    type: "website",
    url: "https://el-enderj.github.io/NeoCalculator",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  const schemaData = {
    "@context": "https://schema.org",
    "@type": "SoftwareApplication",
    "name": "NumOS",
    "operatingSystem": "ESP32-S3",
    "applicationCategory": "EducationalApplication",
    "description": "An open-source scientific calculator with a Pro-CAS engine.",
    "offers": {
      "@type": "Offer",
      "price": "15.00",
      "priceCurrency": "USD"
    }
  };

  return (
    <html lang="en">
      <head>
        <script
          type="application/ld+json"
          dangerouslySetInnerHTML={{ __html: JSON.stringify(schemaData) }}
        />
      </head>
      <body className={`${inter.variable} ${jetbrainsMono.variable} font-sans bg-[#0A0A0E] text-white antialiased`}>
        {children}
      </body>
    </html>
  );
}
