import React, { useEffect, useState } from 'react';
import {
  Globe,
  ExternalLink,
  Search,
  BookOpen,
  Code2,
  FileText,
  HelpCircle,
  Sparkles,
  AlertTriangle,
  RefreshCw,
  Cpu
} from 'lucide-react';
import { sounds } from '../utils/sound';

const GOOGLE_PSE_CX = 'c3865c5b66ea34ecb';
const SCRIPT_ID = 'google-pse-script';

export const ResourceExplorerPage: React.FC = () => {
  const [scriptLoaded, setScriptLoaded] = useState(false);
  const [scriptError, setScriptError] = useState(false);

  useEffect(() => {
    let checkInterval: ReturnType<typeof setInterval> | null = null;

    // Check if the Google CSE element API is ready and trigger render
    const initializeGcse = () => {
      const google = (window as any).google;
      if (google?.search?.cse?.element) {
        google.search.cse.element.go();
        setScriptLoaded(true);
        return true;
      }
      return false;
    };

    // Check if script already exists in document
    const existingScript = document.getElementById(SCRIPT_ID) as HTMLScriptElement | null;

    if (existingScript) {
      if (!initializeGcse()) {
        // Poll for initialization if script tag exists but engine is not yet ready
        checkInterval = setInterval(() => {
          if (initializeGcse() && checkInterval) {
            clearInterval(checkInterval);
          }
        }, 150);
      }
    } else {
      // Create script tag once
      const script = document.createElement('script');
      script.id = SCRIPT_ID;
      script.src = `https://cse.google.com/cse.js?cx=${GOOGLE_PSE_CX}`;
      script.async = true;
      script.onload = () => {
        setScriptLoaded(true);
        initializeGcse();
      };
      script.onerror = () => {
        setScriptError(true);
      };
      document.head.appendChild(script);
    }

    // Safety timeout to stop polling after 5 seconds
    const timeout = setTimeout(() => {
      if (checkInterval) clearInterval(checkInterval);
      if (!(window as any).google?.search?.cse) {
        // If not loaded, script might be blocked by adblocker
        setScriptError(false); // Graceful fallback
      }
    }, 5000);

    return () => {
      if (checkInterval) clearInterval(checkInterval);
      clearTimeout(timeout);
    };
  }, []);

  const resourceCategories = [
    {
      id: 'github',
      name: 'GitHub Repositories',
      icon: <Code2 className="w-4 h-4 text-emerald-400" />,
      desc: 'Open-source C11 data structure implementations, benchmark suites, and engine architectures.',
      query: 'C data structures algorithms implementation GitHub',
      url: 'https://github.com/search?q=C+data+structures+algorithms&type=repositories',
    },
    {
      id: 'gfg',
      name: 'GeeksforGeeks & Tutorials',
      icon: <BookOpen className="w-4 h-4 text-teal-400" />,
      desc: 'Step-by-step algorithmic explanations, Trie node structures, and Min Heap property proofs.',
      query: 'Trie Min Heap autocomplete GeeksforGeeks',
      url: 'https://www.geeksforgeeks.org/search/?q=autocomplete+trie+min+heap',
    },
    {
      id: 'stackoverflow',
      name: 'Stack Overflow Discussions',
      icon: <HelpCircle className="w-4 h-4 text-amber-400" />,
      desc: 'Practical engineering Q&A, cache invalidation, separate chaining, and C memory optimization.',
      query: 'Trie cache autocomplete frequency ranking Stack Overflow',
      url: 'https://stackoverflow.com/search?q=autocomplete+trie+ranking',
    },
    {
      id: 'docs',
      name: 'Technical Documentation',
      icon: <FileText className="w-4 h-4 text-cyan-400" />,
      desc: 'Language standards (ISO/IEC 9899 C11), POSIX threading, and SQLite C API specifications.',
      query: 'SQLite C API documentation civetweb cJSON',
      url: 'https://www.sqlite.org/c3ref/intro.html',
    },
    {
      id: 'research',
      name: 'Academic Research Papers',
      icon: <Sparkles className="w-4 h-4 text-indigo-400" />,
      desc: 'Peer-reviewed publications on compressed Trie variants, top-k query processing, and inverted index retrieval.',
      query: 'prefix search top-k queries trie data structure research paper',
      url: 'https://scholar.google.com/scholar?q=prefix+search+top-k+trie+cache',
    },
  ];

  return (
    <div className="max-w-6xl mx-auto px-4 sm:px-6 py-8">
      {/* Header */}
      <div className="flex flex-wrap items-center justify-between gap-4 mb-6">
        <div>
          <div className="inline-flex items-center space-x-2 px-2.5 py-0.5 rounded-full bg-slate-900 border border-slate-800 text-[11px] font-mono text-slate-400 mb-2">
            <Globe className="w-3.5 h-3.5 text-blue-400" />
            <span>External Discovery & Developer Hub</span>
          </div>
          <h1 className="text-2xl sm:text-3xl font-extrabold text-white tracking-tight flex items-center space-x-3">
            <span>Resource Explorer</span>
            <span className="text-xs font-mono font-normal px-2.5 py-0.5 rounded-full bg-blue-500/10 text-blue-400 border border-blue-500/20">
              Google PSE Powered
            </span>
          </h1>
          <p className="text-slate-400 text-xs sm:text-sm mt-1 max-w-2xl leading-relaxed">
            Discover GitHub repositories, technical documentation, GeeksforGeeks tutorials, Stack Overflow discussions, and scientific research publications.
          </p>
        </div>

        {/* Engine Configuration Badge */}
        <div className="flex items-center space-x-2 bg-slate-900/80 border border-slate-800 rounded-xl px-3 py-2 text-xs font-mono text-slate-400">
          <span className="text-slate-500">Search Engine ID:</span>
          <span className="text-blue-400 font-bold">{GOOGLE_PSE_CX}</span>
        </div>
      </div>

      {/* Architectural Separation Notice */}
      <div className="mb-6 p-4 rounded-xl bg-slate-900/60 border border-slate-800 flex items-start space-x-3 text-xs text-slate-300">
        <Cpu className="w-4 h-4 text-teal-400 flex-shrink-0 mt-0.5" />
        <div className="space-y-1">
          <p className="font-semibold text-slate-200">
            Architectural Separation Notice:
          </p>
          <p className="text-slate-400 leading-relaxed text-[11px]">
            Resource Explorer is an independent supporting feature for external technical resource discovery.
            It does not alter internal keyword popularity, affect Trie Top-10 caches, or modify C engine rankings.
            The core IntelliSuggest autocomplete engine operates entirely on custom in-memory C algorithms (Trie, Hash Table, Min Heap).
          </p>
        </div>
      </div>

      {/* Script Loading Fallback Alert if blocked */}
      {scriptError && (
        <div className="mb-6 p-4 rounded-xl bg-amber-500/10 border border-amber-500/30 flex items-center justify-between text-xs text-amber-300">
          <div className="flex items-center space-x-2">
            <AlertTriangle className="w-4 h-4 flex-shrink-0" />
            <span>
              Google Programmable Search Engine script was blocked or unreachable (network restriction or adblocker). Direct discovery portals below remain accessible.
            </span>
          </div>
          <button
            onClick={() => window.location.reload()}
            className="flex items-center space-x-1 px-2.5 py-1 rounded bg-amber-500/20 hover:bg-amber-500/30 text-amber-200 font-medium"
          >
            <RefreshCw className="w-3 h-3" />
            <span>Retry</span>
          </button>
        </div>
      )}

      {/* Main Google Programmable Search Engine Container */}
      <div className="bg-slate-900/70 border border-slate-800 rounded-2xl p-6 shadow-2xl mb-8 backdrop-blur">
        <div className="flex items-center justify-between border-b border-slate-800/80 pb-4 mb-5">
          <div className="flex items-center space-x-2">
            <Search className="w-4 h-4 text-blue-400" />
            <h2 className="text-sm font-bold text-white uppercase tracking-wider">
              Google Programmable Search Engine
            </h2>
          </div>
          <span className="text-[11px] font-mono text-slate-500">
            Live Web Results
          </span>
        </div>

        {/* Exact Google PSE Search Element Container */}
        <div className="min-h-[140px] text-slate-200 rounded-xl overflow-hidden bg-slate-950/60 p-3 border border-slate-800/80">
          <div className="gcse-search" data-enableAutoComplete="true" data-autoSearchOnLoad="false"></div>
          {!scriptLoaded && !scriptError && (
            <div className="flex items-center justify-center py-8 text-xs text-slate-500 space-x-2">
              <RefreshCw className="w-3.5 h-3.5 animate-spin text-blue-400" />
              <span>Initializing Google Programmable Search Engine widget...</span>
            </div>
          )}
        </div>
      </div>

      {/* Curated Technical Discovery Hubs */}
      <div className="mb-8">
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-sm font-bold text-slate-300 uppercase tracking-wider flex items-center space-x-2">
            <BookOpen className="w-4 h-4 text-teal-400" />
            <span>Curated Developer Resource Portals</span>
          </h2>
          <span className="text-xs font-mono text-slate-500">Quick Access</span>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {resourceCategories.map((cat) => (
            <div
              key={cat.id}
              className="bg-slate-900/60 border border-slate-800 hover:border-slate-700 rounded-2xl p-5 shadow-lg flex flex-col justify-between transition-all group"
            >
              <div>
                <div className="flex items-center justify-between mb-2">
                  <div className="flex items-center space-x-2">
                    {cat.icon}
                    <h3 className="text-sm font-bold text-white group-hover:text-blue-300 transition-colors">
                      {cat.name}
                    </h3>
                  </div>
                  <a
                    href={cat.url}
                    target="_blank"
                    rel="noopener noreferrer"
                    onClick={() => sounds.playSelect()}
                    className="p-1 rounded text-slate-500 hover:text-white transition-colors"
                    title={`Open ${cat.name} in new tab`}
                  >
                    <ExternalLink className="w-3.5 h-3.5" />
                  </a>
                </div>
                <p className="text-xs text-slate-400 leading-relaxed mb-4">
                  {cat.desc}
                </p>
              </div>

              <div className="pt-3 border-t border-slate-800/80 flex items-center justify-between">
                <span className="text-[11px] font-mono text-slate-500 truncate max-w-[180px]">
                  {cat.query}
                </span>
                <a
                  href={cat.url}
                  target="_blank"
                  rel="noopener noreferrer"
                  onClick={() => sounds.playSelect()}
                  className="inline-flex items-center space-x-1 text-[11px] font-semibold text-blue-400 hover:text-blue-300 transition-colors"
                >
                  <span>Explore</span>
                  <ExternalLink className="w-3 h-3" />
                </a>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
