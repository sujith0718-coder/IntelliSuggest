import React, { useState, useEffect, useRef } from 'react';
import { api, Suggestion, ConceptResult, PopularItem, HistoryItem } from '../services/api';
import { GoogleSearchIntegration } from '../components/GoogleSearchIntegration';
import { sounds } from '../utils/sound';
import {
  Search as SearchIcon,
  Sparkles,
  Clock,
  TrendingUp,
  Zap,
  ArrowRight,
  CornerDownLeft,
  Database,
  Globe,
  SlidersHorizontal,
  ChevronRight,
  Activity
} from 'lucide-react';

interface SearchPageProps {
  searchInputRef?: React.RefObject<HTMLInputElement>;
  initialMode?: 'prefix' | 'forgotten';
}

export const SearchPage: React.FC<SearchPageProps> = ({ searchInputRef, initialMode }) => {
  const [query, setQuery] = useState('');
  const [searchMode, setSearchMode] = useState<'prefix' | 'forgotten'>(initialMode || 'prefix');
  const [rankingMode, setRankingMode] = useState<'cached' | 'heap'>('cached');

  useEffect(() => {
    if (initialMode && initialMode !== searchMode) {
      setSearchMode(initialMode);
    }
  }, [initialMode]);

  const [suggestions, setSuggestions] = useState<Suggestion[]>([]);
  const [conceptResults, setConceptResults] = useState<ConceptResult[]>([]);
  const [latencyUs, setLatencyUs] = useState<number | null>(null);
  const [popular, setPopular] = useState<PopularItem[]>([]);
  const [history, setHistory] = useState<HistoryItem[]>([]);
  const [selectedIndex, setSelectedIndex] = useState<number>(-1);

  // Submitted keyword tracking (activates search results & Google CSE integration)
  const [submittedQuery, setSubmittedQuery] = useState<string | null>(null);
  const [submittedFreq, setSubmittedFreq] = useState<number | null>(null);
  const [activeResultsTab, setActiveResultsTab] = useState<'dsa' | 'web'>('dsa');
  const [isLoading, setIsLoading] = useState(false);

  const localInputRef = useRef<HTMLInputElement>(null);
  const effectiveInputRef = (searchInputRef || localInputRef) as React.RefObject<HTMLInputElement>;

  const loadSidebarData = async () => {
    try {
      const popRes = await api.getPopular();
      setPopular(popRes.popular || []);
      const histRes = await api.getHistory();
      setHistory(histRes.history || []);
    } catch (err) {
      console.error('Failed to load initial data:', err);
    }
  };

  useEffect(() => {
    loadSidebarData();
  }, []);

  // Fetch suggestions when query changes in prefix mode
  useEffect(() => {
    if (searchMode !== 'prefix') {
      setSuggestions([]);
      return;
    }

    if (query.trim().length === 0) {
      setSuggestions([]);
      setLatencyUs(null);
      setSelectedIndex(-1);
      return;
    }

    let isSubscribed = true;
    api.getSuggestions(query, rankingMode)
      .then((res) => {
        if (isSubscribed) {
          setSuggestions(res.suggestions || []);
          setLatencyUs(res.latency_us);
          setSelectedIndex(-1);
        }
      })
      .catch((err) => console.error(err));

    return () => {
      isSubscribed = false;
    };
  }, [query, searchMode, rankingMode]);

  // Execute Forgotten-Name Search
  const handleForgottenSearch = async () => {
    if (!query.trim()) return;
    setIsLoading(true);
    sounds.playSubmit();
    try {
      const res = await api.forgottenSearch(query);
      setConceptResults(res.results || []);
      setSubmittedQuery(query);
    } catch (err) {
      console.error(err);
    } finally {
      setIsLoading(false);
    }
  };

  // Submit search (triggers C Hash Table increment, SQLite persistence, and Web Results)
  const handleSubmitKeyword = async (keyword: string) => {
    if (!keyword.trim()) return;
    sounds.playSubmit();
    try {
      const res = await api.submitSearch(keyword);
      setSubmittedQuery(keyword);
      setSubmittedFreq(res.new_frequency);
      setSuggestions([]);
      setSelectedIndex(-1);
      loadSidebarData();
    } catch (err) {
      console.error('Submit failed:', err);
    }
  };

  // Keyboard navigation inside suggestions list
  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (searchMode === 'forgotten') {
      if (e.key === 'Enter') {
        e.preventDefault();
        handleForgottenSearch();
      }
      return;
    }

    if (suggestions.length === 0) {
      if (e.key === 'Enter' && query.trim()) {
        handleSubmitKeyword(query);
      }
      return;
    }

    if (e.key === 'ArrowDown') {
      e.preventDefault();
      sounds.playNav();
      setSelectedIndex((prev) => (prev < suggestions.length - 1 ? prev + 1 : 0));
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      sounds.playNav();
      setSelectedIndex((prev) => (prev > 0 ? prev - 1 : suggestions.length - 1));
    } else if (e.key === 'Enter') {
      e.preventDefault();
      const chosen = selectedIndex >= 0 ? suggestions[selectedIndex].keyword : query;
      handleSubmitKeyword(chosen);
    } else if (e.key === 'Escape') {
      sounds.playNav();
      setSuggestions([]);
      setSelectedIndex(-1);
    }
  };

  // Helper to highlight matching prefix in suggestion keyword
  const renderHighlightedKeyword = (fullKeyword: string, currentPrefix: string) => {
    const cleanPrefix = currentPrefix.trim().toLowerCase();
    if (!cleanPrefix || !fullKeyword.toLowerCase().startsWith(cleanPrefix)) {
      return <span>{fullKeyword}</span>;
    }
    const matchedPart = fullKeyword.slice(0, cleanPrefix.length);
    const remainingPart = fullKeyword.slice(cleanPrefix.length);

    return (
      <span className="flex items-baseline">
        <span className="text-teal-400 font-bold tracking-tight">{matchedPart}</span>
        <span className="text-white font-medium">{remainingPart}</span>
      </span>
    );
  };

  return (
    <div className="max-w-5xl mx-auto px-4 sm:px-6 py-8">
      {/* Hero Header with Linear/Raycast aesthetic */}
      <div className="text-center mb-8">
        <div className="inline-flex items-center space-x-2 px-3 py-1 rounded-full bg-slate-900/90 border border-slate-800 text-xs font-mono text-slate-400 mb-4 shadow-inner">
          <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse" />
          <span>Sub-Microsecond In-Memory Search Engine</span>
        </div>
        <h1 className="text-3xl sm:text-5xl font-black text-white tracking-tight">
          Intelli<span className="text-transparent bg-clip-text bg-gradient-to-r from-teal-400 to-emerald-400">Suggest</span>
        </h1>
        <p className="mt-2 text-slate-400 text-sm sm:text-base max-w-2xl mx-auto leading-relaxed">
          Full-stack academic engine powered by a custom <span className="text-teal-400 font-mono">Trie</span> for $O(L)$ prefix matching, a <span className="text-teal-400 font-mono">Hash Table</span> for frequency updates, and a <span className="text-teal-400 font-mono">Min Heap</span> for Top-10 selection.
        </p>
      </div>

      {/* Mode Controls Bar */}
      <div className="flex flex-wrap items-center justify-between gap-3 bg-slate-900/60 border border-slate-800/80 rounded-2xl p-2.5 mb-6 backdrop-blur shadow-sm">
        <div className="flex items-center space-x-1.5">
          <button
            onClick={() => {
              sounds.playModeChange();
              setSearchMode('prefix');
              setConceptResults([]);
            }}
            className={`flex items-center space-x-1.5 px-3 py-1.5 rounded-xl text-xs font-semibold transition-all ${
              searchMode === 'prefix'
                ? 'bg-teal-500 text-slate-950 shadow-md shadow-teal-500/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <SearchIcon className="w-3.5 h-3.5" />
            <span>Prefix Autocomplete</span>
          </button>

          <button
            onClick={() => {
              sounds.playModeChange();
              setSearchMode('forgotten');
              setSuggestions([]);
            }}
            className={`flex items-center space-x-1.5 px-3 py-1.5 rounded-xl text-xs font-semibold transition-all ${
              searchMode === 'forgotten'
                ? 'bg-teal-500 text-slate-950 shadow-md shadow-teal-500/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <Sparkles className="w-3.5 h-3.5" />
            <span>Forgotten-Name Search</span>
          </button>
        </div>

        {searchMode === 'prefix' && (
          <div className="flex items-center space-x-1.5 bg-slate-950/60 p-1 rounded-xl border border-slate-800/80 text-xs font-mono">
            <span className="text-[11px] text-slate-500 px-2 flex items-center space-x-1">
              <SlidersHorizontal className="w-3 h-3 text-slate-400" />
              <span>Algorithm:</span>
            </span>
            <button
              onClick={() => {
                sounds.playSelect();
                setRankingMode('cached');
              }}
              className={`px-2.5 py-1 rounded-lg transition-all ${
                rankingMode === 'cached'
                  ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/30 font-bold'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              Approach B (Cache)
            </button>
            <button
              onClick={() => {
                sounds.playSelect();
                setRankingMode('heap');
              }}
              className={`px-2.5 py-1 rounded-lg transition-all ${
                rankingMode === 'heap'
                  ? 'bg-sky-500/20 text-sky-300 border border-sky-500/30 font-bold'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              Approach A (Heap)
            </button>
          </div>
        )}
      </div>

      {/* Main Search Command Palette Input */}
      <div className="relative mb-6">
        <div className="relative flex items-center">
          <div className="absolute left-4 pointer-events-none text-slate-400">
            {searchMode === 'prefix' ? (
              <SearchIcon className="w-5 h-5 text-slate-400" />
            ) : (
              <Sparkles className="w-5 h-5 text-teal-400 animate-spin-slow" />
            )}
          </div>
          <input
            ref={effectiveInputRef}
            type="text"
            id="mainSearchInput"
            name="searchQuery"
            autoComplete="off"
            spellCheck={false}
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder={
              searchMode === 'prefix'
                ? "Type a prefix (e.g. 'pro', 'data', 'py', 'algo')..."
                : "Describe what you remember (e.g. 'connect laptop to projector', 'store previous results')..."
            }
            className="w-full pl-12 pr-32 py-4 bg-slate-900/90 border border-slate-800 focus:border-teal-500 focus:ring-4 focus:ring-teal-500/15 rounded-2xl text-white placeholder-slate-500 text-base shadow-2xl outline-none transition-all font-sans"
            autoFocus
          />
          <div className="absolute right-3 flex items-center space-x-2">
            {searchMode === 'forgotten' ? (
              <button
                onClick={handleForgottenSearch}
                disabled={isLoading}
                className="px-4 py-2 bg-teal-500 hover:bg-teal-400 text-slate-950 font-bold text-xs rounded-xl transition-all shadow-md flex items-center space-x-1.5"
              >
                <span>Find Term</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </button>
            ) : (
              <div className="hidden sm:flex items-center space-x-1.5 text-xs text-slate-400 bg-slate-950 px-3 py-1.5 rounded-xl border border-slate-800/80 font-mono">
                <span>Enter</span>
                <CornerDownLeft className="w-3.5 h-3.5 text-teal-400" />
              </div>
            )}
          </div>
        </div>

        {/* Live microsecond latency chip */}
        {latencyUs !== null && searchMode === 'prefix' && (
          <div className="mt-2.5 flex items-center justify-between text-xs text-slate-400 px-3 font-mono">
            <span className="flex items-center space-x-1.5">
              <Zap className="w-3.5 h-3.5 text-amber-400" />
              <span>
                C-Engine Latency: <strong className="text-emerald-400 font-bold">{latencyUs.toFixed(1)} μs</strong>
              </span>
            </span>
            <span className="text-slate-500">
              {rankingMode === 'cached' ? 'Trie Node Top-10 Cache · O(L + K)' : 'Subtree DFS + Min Heap · O(M log K)'}
            </span>
          </div>
        )}

        {/* Prefix Suggestions Dropdown */}
        {suggestions.length > 0 && searchMode === 'prefix' && (
          <div className="absolute left-0 right-0 mt-2 bg-slate-950/95 border border-slate-800 rounded-2xl shadow-2xl overflow-hidden z-40 backdrop-blur-md animate-in fade-in-50 duration-150">
            <div className="px-4 py-2.5 bg-slate-900/60 border-b border-slate-800/80 flex items-center justify-between text-[11px] text-slate-400 font-mono uppercase tracking-wider">
              <span>Top-{suggestions.length} Suggestions (Min Heap Ranked)</span>
              <span>Search Frequency</span>
            </div>
            <ul className="divide-y divide-slate-800/50">
              {suggestions.map((item, idx) => {
                const isSelected = selectedIndex === idx;
                const isTop1 = idx === 0;
                const isTop2 = idx === 1;
                const isTop3 = idx === 2;

                return (
                  <li
                    key={idx}
                    onClick={() => handleSubmitKeyword(item.keyword)}
                    onMouseEnter={() => {
                      sounds.playNav();
                      setSelectedIndex(idx);
                    }}
                    className={`px-4 py-3 flex items-center justify-between cursor-pointer transition-colors ${
                      isSelected
                        ? 'bg-teal-500/10 border-l-4 border-teal-500 pl-3'
                        : 'hover:bg-slate-900/60'
                    }`}
                  >
                    <div className="flex items-center space-x-3">
                      <span
                        className={`w-6 h-6 rounded-lg text-xs flex items-center justify-center font-mono font-bold ${
                          isTop1
                            ? 'bg-amber-500/20 text-amber-300 border border-amber-500/30'
                            : isTop2
                            ? 'bg-slate-300/20 text-slate-200 border border-slate-300/30'
                            : isTop3
                            ? 'bg-orange-700/20 text-orange-400 border border-orange-700/30'
                            : 'bg-slate-800 text-slate-400'
                        }`}
                      >
                        #{idx + 1}
                      </span>
                      {renderHighlightedKeyword(item.keyword, query)}
                    </div>
                    <div className="flex items-center space-x-2">
                      <span className="text-xs px-2.5 py-0.5 rounded-full bg-slate-900 border border-slate-800 text-teal-400 font-mono font-bold">
                        {item.frequency} searches
                      </span>
                      <ChevronRight className="w-3.5 h-3.5 text-slate-600" />
                    </div>
                  </li>
                );
              })}
            </ul>
          </div>
        )}
      </div>

      {/* Submitted Search View: C-Engine Insights + Google Programmable Search */}
      {submittedQuery && (
        <div className="mb-8 space-y-4">
          <div className="flex flex-wrap items-center justify-between gap-3 bg-slate-900/80 border border-teal-500/30 rounded-2xl p-4 shadow-xl">
            <div className="flex items-center space-x-3">
              <div className="w-10 h-10 rounded-xl bg-teal-500/20 border border-teal-500/30 flex items-center justify-center">
                <Database className="w-5 h-5 text-teal-400" />
              </div>
              <div>
                <span className="text-[11px] font-mono uppercase tracking-wider text-teal-400 font-semibold">
                  Search Query Executed
                </span>
                <h3 className="text-lg font-bold text-white tracking-tight">"{submittedQuery}"</h3>
              </div>
            </div>

            {/* View Switcher Tabs: DSA In-Memory vs. Google Programmable Search */}
            <div className="flex items-center space-x-2 bg-slate-950/80 p-1 rounded-xl border border-slate-800 text-xs font-semibold">
              <button
                onClick={() => {
                  sounds.playSelect();
                  setActiveResultsTab('dsa');
                }}
                className={`flex items-center space-x-1.5 px-3 py-1.5 rounded-lg transition-all ${
                  activeResultsTab === 'dsa'
                    ? 'bg-teal-500 text-slate-950 shadow-md font-bold'
                    : 'text-slate-400 hover:text-white'
                }`}
              >
                <Activity className="w-3.5 h-3.5" />
                <span>C-Engine Metadata</span>
              </button>

              <button
                onClick={() => {
                  sounds.playSelect();
                  setActiveResultsTab('web');
                }}
                className={`flex items-center space-x-1.5 px-3 py-1.5 rounded-lg transition-all ${
                  activeResultsTab === 'web'
                    ? 'bg-blue-600 text-white shadow-md font-bold'
                    : 'text-slate-400 hover:text-white'
                }`}
              >
                <Globe className="w-3.5 h-3.5 text-blue-400" />
                <span>Google Web Results</span>
              </button>
            </div>
          </div>

          {/* Sub-view: C-Engine Metadata */}
          {activeResultsTab === 'dsa' && (
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
              <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-4">
                <span className="text-xs text-slate-400">Updated Popularity:</span>
                <div className="mt-1 flex items-baseline justify-between">
                  <span className="text-2xl font-bold font-mono text-teal-400">
                    {submittedFreq ? `${submittedFreq} hits` : 'Recorded'}
                  </span>
                  <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-emerald-500/10 text-emerald-400 border border-emerald-500/20">
                    +1 Popularity
                  </span>
                </div>
                <p className="mt-1 text-[11px] text-slate-500">Persisted in SQLite and Hash Table</p>
              </div>

              <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-4">
                <span className="text-xs text-slate-400">Trie Top-10 Cache Status:</span>
                <div className="mt-1 text-2xl font-bold font-mono text-emerald-400">Synchronized</div>
                <p className="mt-1 text-[11px] text-slate-500">Ancestor nodes recomputed upwards</p>
              </div>

              <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-4">
                <span className="text-xs text-slate-400">Search History Audit:</span>
                <div className="mt-1 text-2xl font-bold font-mono text-white">Logged</div>
                <p className="mt-1 text-[11px] text-slate-500">Appended to search_history table</p>
              </div>
            </div>
          )}

          {/* Sub-view: Google Programmable Search Integration */}
          {activeResultsTab === 'web' && (
            <GoogleSearchIntegration query={submittedQuery} />
          )}
        </div>
      )}

      {/* Forgotten-Name Results View */}
      {searchMode === 'forgotten' && conceptResults.length > 0 && (
        <div className="space-y-4 mb-8">
          <h2 className="text-base font-bold text-white flex items-center space-x-2">
            <Sparkles className="w-4 h-4 text-teal-400" />
            <span>Retrieved Concept Matches (Inverted Index Scoring)</span>
          </h2>
          <div className="grid grid-cols-1 gap-4">
            {conceptResults.map((c, i) => (
              <div
                key={i}
                className="bg-slate-900/70 border border-slate-800 rounded-2xl p-5 hover:border-teal-500/50 transition-all shadow-lg backdrop-blur"
              >
                <div className="flex items-start justify-between">
                  <div>
                    <h3 className="text-lg font-bold text-teal-400">{c.concept_name}</h3>
                    <p className="mt-1.5 text-sm text-slate-300 leading-relaxed">{c.description}</p>
                  </div>
                  <div className="text-right">
                    <span className="px-3 py-1 rounded-full text-xs font-mono font-bold bg-teal-500/10 text-teal-300 border border-teal-500/30">
                      Relevance: {(c.relevance_score * 100).toFixed(0)}%
                    </span>
                  </div>
                </div>

                {c.related_keywords.length > 0 && (
                  <div className="mt-4 pt-3 border-t border-slate-800/80 flex flex-wrap items-center gap-2">
                    <span className="text-xs text-slate-500 font-medium">Related keywords:</span>
                    {c.related_keywords.map((kw, kidx) => (
                      <button
                        key={kidx}
                        onClick={() => {
                          sounds.playSelect();
                          setSearchMode('prefix');
                          setQuery(kw);
                          handleSubmitKeyword(kw);
                        }}
                        className="text-xs bg-slate-950 hover:bg-teal-500/20 hover:text-teal-300 text-slate-300 px-3 py-1 rounded-lg border border-slate-800 transition-all font-mono"
                      >
                        {kw}
                      </button>
                    ))}
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Popular Searches & Search History Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mt-8">
        {/* Popular searches card */}
        <div className="bg-slate-900/40 border border-slate-800/80 rounded-2xl p-5 shadow-lg">
          <div className="flex items-center justify-between mb-4">
            <div className="flex items-center space-x-2 text-slate-200">
              <TrendingUp className="w-4 h-4 text-teal-400" />
              <h3 className="font-bold text-sm">Most Popular Keywords</h3>
            </div>
            <span className="text-[11px] font-mono text-slate-500">Live Frequency</span>
          </div>

          <div className="space-y-2">
            {popular.length === 0 ? (
              <p className="text-xs text-slate-500 py-4 text-center">No popular keywords recorded yet.</p>
            ) : (
              popular.slice(0, 6).map((item, idx) => (
                <div
                  key={item.id}
                  onClick={() => {
                    sounds.playSelect();
                    setSearchMode('prefix');
                    setQuery(item.keyword);
                    handleSubmitKeyword(item.keyword);
                  }}
                  className="flex items-center justify-between p-2.5 rounded-xl hover:bg-slate-800/60 cursor-pointer transition-colors text-sm group"
                >
                  <div className="flex items-center space-x-2.5">
                    <span className="w-5 h-5 rounded bg-slate-800 text-slate-400 text-[11px] flex items-center justify-center font-mono">
                      {idx + 1}
                    </span>
                    <span className="text-slate-200 group-hover:text-teal-400 transition-colors">
                      {item.keyword}
                    </span>
                  </div>
                  <span className="text-xs font-mono text-slate-400 bg-slate-950 px-2.5 py-0.5 rounded-lg border border-slate-800">
                    {item.frequency}
                  </span>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Recent search audit log */}
        <div className="bg-slate-900/40 border border-slate-800/80 rounded-2xl p-5 shadow-lg">
          <div className="flex items-center justify-between mb-4">
            <div className="flex items-center space-x-2 text-slate-200">
              <Clock className="w-4 h-4 text-teal-400" />
              <h3 className="font-bold text-sm">Recent Submitted Searches</h3>
            </div>
            <span className="text-[11px] font-mono text-slate-500">Audit History</span>
          </div>

          <div className="space-y-2">
            {history.length === 0 ? (
              <p className="text-xs text-slate-500 py-4 text-center">No search history yet. Submit a search above.</p>
            ) : (
              history.slice(0, 6).map((item) => (
                <div
                  key={item.id}
                  onClick={() => {
                    sounds.playSelect();
                    setSearchMode('prefix');
                    setQuery(item.keyword);
                  }}
                  className="flex items-center justify-between p-2.5 rounded-xl hover:bg-slate-800/60 cursor-pointer transition-colors text-sm group"
                >
                  <span className="text-slate-200 group-hover:text-teal-400 transition-colors">
                    {item.keyword}
                  </span>
                  <span className="text-xs font-mono text-slate-500">
                    {item.searched_at ? new Date(item.searched_at).toLocaleTimeString() : 'Recent'}
                  </span>
                </div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
