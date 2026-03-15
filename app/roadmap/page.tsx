import fs from 'fs';
import path from 'path';

interface Milestone {
  date: string;
  desc: string;
  status: 'done' | 'current' | 'upcoming';
}

function parseRoadmap(): Milestone[] {
  const filePath = path.join(process.cwd(), 'docs', 'ROADMAP.md');
  let fileContents = '';
  
  try {
    fileContents = fs.readFileSync(filePath, 'utf8');
  } catch (e) {
    return [];
  }

  const milestones: Milestone[] = [];
  
  // 1. Extract Milestone History (Done / Current)
  const historySplit = fileContents.split('## Milestone History');
  if (historySplit.length > 1) {
    const historyText = historySplit[1].split('---')[0];
    const lines = historyText.trim().split('\n');
    
    for (const line of lines) {
      if (line.trim().startsWith('|') && !line.includes('Date | Milestone') && !line.includes(':------|:-----')) {
         const cols = line.split('|').filter(c => c.trim() !== '');
         if (cols.length >= 2) {
           const date = cols[0].trim().replace(/\*\*/g, '');
           const desc = cols[1].trim().replace(/\*\*/g, '');
           // Find if it's the very last item in history, mark as "current"
           milestones.push({ date, desc, status: 'done' });
         }
      }
    }
  }

  if (milestones.length > 0) {
    milestones[milestones.length - 1].status = 'current'; // Mark the latest as current
  }

  // 2. Extract Future Phases natively (Upcoming)
  const regex = /## Phase \d+ — (.*?) \((.*?)\)/g;
  let match;
  while ((match = regex.exec(fileContents)) !== null) {
    const desc = match[1].trim();
    const statusText = match[2].trim().toLowerCase();
    
    if (statusText.includes('planned') || statusText.includes('in progress')) {
      milestones.push({
        date: 'Upcoming',
        desc: desc,
        status: statusText.includes('in progress') ? 'current' : 'upcoming'
      });
    }
  }

  // 3. Extract NeoLanguage (Phase 9) since it might use a different header
  const regexNeo = /## Phase 9 — (.*?) \((.*?)\)/g;
  while ((match = regexNeo.exec(fileContents)) !== null) {
      if (!milestones.find(m => m.desc === match![1].trim())) {
         milestones.push({
            date: 'Upcoming',
            desc: match[1].trim(),
            status: 'current'
         });
      }
  }

  return milestones;
}

export default async function RoadmapPage() {
  const milestones = parseRoadmap();

  return (
    <main className="max-w-4xl mx-auto px-4 py-24 min-h-screen">
      <div className="text-center mb-16 relative">
         <div className="absolute top-0 left-1/2 -translate-x-1/2 w-32 h-32 bg-[#ccff00]/10 blur-[50px] -z-10 rounded-full"></div>
         <h1 className="text-5xl md:text-7xl font-black tracking-tighter mb-4 text-center text-white drop-shadow-lg">
           Trajectory.
         </h1>
         <p className="text-[#00D1FF] font-mono tracking-widest uppercase">Live from ROADMAP.md</p>
      </div>

      <div className="relative border-l-2 border-white/10 ml-4 md:ml-12 pl-8 space-y-12">
        {milestones.map((m, i) => (
          <div key={i} className="relative group">
            <div className={`absolute -left-[33px] top-4 w-4 h-4 rounded-full border-2 bg-[#0a0a0a] transition-colors
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
              <p className={`font-sans ${m.status === 'done' || m.status === 'current' ? 'text-gray-200 font-medium' : 'text-gray-500'}`}>{m.desc}</p>
            </div>
          </div>
        ))}
      </div>
    </main>
  );
}
