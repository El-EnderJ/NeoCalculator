export default function RoadmapPage() {
  const milestones = [
    { title: "Project Genesis", date: "Q1 2025", desc: "ESP32-S3 hardware evaluation and initial layout.", status: "done" },
    { title: "NumOS Core", date: "Q2 2025", desc: "Bare-metal C++17 foundation and LVGL driver bridge.", status: "done" },
    { title: "Pro-CAS Engine", date: "Q3 2025", desc: "Symbolic Algebra System capable of executing within 97KB SRAM.", status: "done" },
    { title: "UI & Layout Overhaul", date: "Q4 2025", desc: "60FPS interface implementation and V.P.A.M parser.", status: "done" },
    { title: "Kickstarter Preparation", date: "Q2 2026", desc: "Premium web presence, cost analysis, and marketing push.", status: "current" },
    { title: "Pilot Manufacturing", date: "Q3 2026", desc: "First 100 PCB batch assembly and QA testing.", status: "upcoming" },
    { title: "Mass Availability", date: "Q1 2027", desc: "Targeted $15 global retail via open-source partner network.", status: "upcoming" },
  ];

  return (
    <main className="max-w-4xl mx-auto px-4 py-24 min-h-screen">
      <h1 className="text-5xl md:text-7xl font-black tracking-tighter mb-16 text-center text-white">
        Trajectory.
      </h1>

      <div className="relative border-l-2 border-white/10 ml-4 md:ml-12 pl-8 space-y-16">
        {milestones.map((m, i) => (
          <div key={i} className="relative group">
            <div className={`absolute -left-[41px] top-1 w-4 h-4 rounded-full border-2 bg-[#0a0a0a] transition-colors
              ${m.status === 'done' ? 'border-[#ccff00] bg-[#ccff00]' : 
                m.status === 'current' ? 'border-[#00D1FF] shadow-[0_0_15px_#00D1FF]' : 
                'border-gray-600'}`}
            ></div>
            
            <div className={`p-6 rounded-2xl border transition-all duration-300
              ${m.status === 'done' ? 'border-white/10 bg-white/5 hover:border-[#ccff00]/50' : 
                m.status === 'current' ? 'border-[#00D1FF]/50 bg-[#00D1FF]/5 shadow-[0_0_30px_rgba(0,209,255,0.05)]' : 
                'border-transparent opacity-50'}`}
            >
              <span className={`text-xs font-mono mb-2 block uppercase tracking-widest
                ${m.status === 'done' ? 'text-[#ccff00]' : 
                  m.status === 'current' ? 'text-[#00D1FF]' : 
                  'text-gray-500'}`}
              >{m.date}</span>
              <h3 className="text-2xl font-bold text-white mb-2">{m.title}</h3>
              <p className="text-gray-400 font-sans">{m.desc}</p>
            </div>
          </div>
        ))}
      </div>
    </main>
  );
}
