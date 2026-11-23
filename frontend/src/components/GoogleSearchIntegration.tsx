import React, { useEffect, useState } from 'react';
import { Globe, ExternalLink, Settings, Sparkles, BookOpen, Search, ArrowUpRight } from 'lucide-react';

interface GoogleSearchProps {
  query: string;
}

export const GoogleSearchIntegration: React.FC<GoogleSearchProps> = ({ query }) => {
  const [cx, setCx] = useState<string>(() => {
    return localStorage.getItem('intellisuggest_google_cx') || '';
  });
  const [showConfig, setShowConfig] = useState(false);
  const [tempCx, setTempCx] = useState(cx);
  const [activeSubTab, setActiveSubTab] = useState<'web' | 'academic' | 'tech'>('web');

  // Load Google CSE script dynamically if CX is configured
  useEffect(() => {
    if (!cx) return;

    const existingScript = document.getElementById('google-cse-script');
    if (!existingScript) {
      const script = document.createElement('script');
      script.id = 'google-cse-script';
      script.src = `https://cse.google.com/cse.js?cx=${cx}`;
      script.async = true;
      document.body.appendChild(script);
    }
  }, [cx]);

  const handleSaveCx = (e: React.FormEvent) => {
    e.preventDefault();
    setCx(tempCx);
    localStorage.setItem('intellisuggest_google_cx', tempCx);
    setShowConfig(false);
  };

  const encodedQuery = encodeURIComponent(query);

  const curatedResults = [
    {
      title: `${query} - Documentation, Concepts & Implementation`,
      snippet: `In-depth technical guides, algorithmic explanations, reference architectures, and community resources related to ${query}.`,
      url: `https://www.google.com/search?q=${encodedQuery}`,
      domain: 'google.com',
      category: 'web',
    },
    {
      title: `${query} on Wikipedia (Encyclopedia Entry)`,
      snippet: `Historical context, formal algorithmic definitions, theoretical complexity analysis, and mathematical formulations for ${query}.`,
      url: `https://en.wikipedia.org/wiki/Special:Search?search=${encodedQuery}`,
      domain: 'wikipedia.org',
      category: 'web',
    },
    {
      title: `Research Papers & Academic Publications on ${query}`,
      snippet: `Peer-reviewed scientific literature, algorithmic performance benchmarks, and conference publications cited on Google Scholar.`,
      url: `https://scholar.google.com/scholar?q=${encodedQuery}`,
      domain: 'scholar.google.com',
      category: 'academic',
    },
    {
      title: `GitHub Open Source Implementations for ${query}`,
      snippet: `High-performance C, C++, and Python repositories, open-source benchmarks, and production libraries implementing ${query}.`,
      url: `https://github.com/search?q=${encodedQuery}&type=repositories`,
      domain: 'github.com',
      category: 'tech',
    },
    {
      title: `Stack Overflow Discussions & Engineering Solutions: ${query}`,
      snippet: `Practical developer Q&A, edge-case troubleshooting, runtime complexity discussions, and debugging tips for ${query}.`,
      url: `https://stackoverflow.com/search?q=${encodedQuery}`,
      domain: 'stackoverflow.com',
      category: 'tech',
    },
  ];

  const filteredResults = curatedResults.filter((r) => {
    if (activeSubTab === 'web') return true;
    return r.category === activeSubTab;
  });

  return (
    <div className="bg-slate-900/70 border border-slate-800 rounded-2xl p-6 shadow-2xl backdrop-blur">
      {/* Header with Google Programmable Search Engine Branding */}
      <div className="flex flex-wrap items-center justify-between gap-3 border-b border-slate-800/80 pb-4 mb-5">
        <div className="flex items-center space-x-3">
          <div className="w-8 h-8 rounded-lg bg-blue-500/10 border border-blue-500/20 flex items-center justify-center">
            <Globe className="w-4 h-4 text-blue-400" />
          </div>
          <div>
            <div className="flex items-center space-x-2">
              <h3 className="text-sm font-bold text-white tracking-tight">Google Programmable Search Integration</h3>
              <span className="text-[10px] font-mono px-2 py-0.5 rounded-full bg-blue-500/10 text-blue-300 border border-blue-500/20">
                Web Results
              </span>
            </div>
            <p className="text-xs text-slate-400">
              Downstream web discovery for query: <strong className="text-teal-400 font-mono">"{query}"</strong>
            </p>
          </div>
        </div>

        <div className="flex items-center space-x-2">
          <button
            onClick={() => setShowConfig(!showConfig)}
            className="flex items-center space-x-1.5 px-2.5 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-300 text-xs font-medium border border-slate-700/60 transition-all"
            title="Configure Google Programmable Search Engine ID (cx)"
          >
            <Settings className="w-3.5 h-3.5 text-slate-400" />
            <span className="hidden sm:inline">{cx ? 'Custom CSE Active' : 'Configure CSE ID'}</span>
          </button>

          <a
            href={`https://www.google.com/search?q=${encodedQuery}`}
            target="_blank"
            rel="noopener noreferrer"
            className="flex items-center space-x-1.5 px-3 py-1.5 rounded-lg bg-blue-600 hover:bg-blue-500 text-white text-xs font-semibold shadow-md transition-all"
          >
            <span>Open in Google</span>
            <ExternalLink className="w-3.5 h-3.5" />
          </a>
        </div>
      </div>

      {/* CSE ID Configuration Modal / Accordion */}
      {showConfig && (
        <form onSubmit={handleSaveCx} className="bg-slate-950/80 border border-slate-800 rounded-xl p-4 mb-5 space-y-3">
          <div className="flex items-start justify-between">
            <div>
              <h4 className="text-xs font-bold text-white flex items-center space-x-1.5">
                <Settings className="w-3.5 h-3.5 text-blue-400" />
                <span>Google Programmable Search Engine (CSE) Settings</span>
              </h4>
              <p className="text-[11px] text-slate-400 mt-0.5">
                Enter your Google Custom Search Engine ID (<code>cx</code>) from <a href="https://programmablesearchengine.google.com" target="_blank" rel="noreferrer" className="text-teal-400 underline">programmablesearchengine.google.com</a> to embed raw Google results directly.
              </p>
            </div>
          </div>
          <div className="flex gap-2">
            <input
              type="text"
              value={tempCx}
              onChange={(e) => setTempCx(e.target.value)}
              placeholder="e.g. 017576662512468239146:omuauf_lfve"
              className="flex-1 px-3 py-2 bg-slate-900 border border-slate-700 rounded-lg text-xs font-mono text-white placeholder-slate-500 outline-none focus:border-blue-500"
            />
            <button
              type="submit"
              className="px-4 py-2 bg-blue-600 hover:bg-blue-500 text-white font-semibold text-xs rounded-lg transition-all"
            >
              Save CSE
            </button>
            {cx && (
              <button
                type="button"
                onClick={() => {
                  setCx('');
                  setTempCx('');
                  localStorage.removeItem('intellisuggest_google_cx');
                }}
                className="px-3 py-2 bg-slate-800 text-slate-400 hover:text-rose-400 text-xs rounded-lg"
              >
                Clear
              </button>
            )}
          </div>
        </form>
      )}

      {/* Embedded Google CSE Widget Container if CX is set */}
      {cx && (
        <div className="mb-6 p-4 rounded-xl bg-slate-950/90 border border-blue-500/30">
          <div className="gcse-searchresults-only" data-queryParameterName="q" data-defaultToRefinement="true" />
        </div>
      )}

      {/* Filter tabs */}
      <div className="flex items-center space-x-2 border-b border-slate-800/80 pb-3 mb-4 text-xs font-medium">
        <button
          onClick={() => setActiveSubTab('web')}
          className={`flex items-center space-x-1.5 px-3 py-1.5 rounded-lg transition-all ${
            activeSubTab === 'web'
              ? 'bg-slate-800 text-white border border-slate-700'
              : 'text-slate-400 hover:text-slate-200'
          }`}
        >
          <Search className="w-3.5 h-3.5" />
          <span>Web & Encyclopedia</span>
        </button>
        <button
          onClick={() => setActiveSubTab('tech')}
          className={`flex items-center space-x-1.5 px-3 py-1.5 rounded-lg transition-all ${
            activeSubTab === 'tech'
              ? 'bg-slate-800 text-white border border-slate-700'
              : 'text-slate-400 hover:text-slate-200'
          }`}
        >
          <Sparkles className="w-3.5 h-3.5 text-teal-400" />
          <span>Technical Repos & Q&A</span>
        </button>
        <button
          onClick={() => setActiveSubTab('academic')}
          className={`flex items-center space-x-1.5 px-3 py-1.5 rounded-lg transition-all ${
            activeSubTab === 'academic'
              ? 'bg-slate-800 text-white border border-slate-700'
              : 'text-slate-400 hover:text-slate-200'
          }`}
        >
          <BookOpen className="w-3.5 h-3.5 text-indigo-400" />
          <span>Academic & Research</span>
        </button>
      </div>

      {/* Result Cards */}
      <div className="space-y-3">
        {filteredResults.map((item, i) => (
          <a
            key={i}
            href={item.url}
            target="_blank"
            rel="noopener noreferrer"
            className="group block p-4 rounded-xl bg-slate-950/40 hover:bg-slate-800/60 border border-slate-800/70 hover:border-blue-500/40 transition-all shadow-sm"
          >
            <div className="flex items-center justify-between">
              <span className="text-[11px] font-mono text-slate-500 group-hover:text-blue-400 transition-colors">
                {item.domain}
              </span>
              <ArrowUpRight className="w-4 h-4 text-slate-600 group-hover:text-blue-400 transition-colors" />
            </div>
            <h4 className="mt-1 text-sm font-semibold text-white group-hover:text-blue-300 transition-colors">
              {item.title}
            </h4>
            <p className="mt-1 text-xs text-slate-400 leading-relaxed">
              {item.snippet}
            </p>
          </a>
        ))}
      </div>
    </div>
  );
};
