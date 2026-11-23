import React, { useState, useEffect, useRef } from 'react';
import { api, Suggestion, ConceptResult, PopularItem, HistoryItem } from '../services/api';
import { Search as SearchIcon, Sparkles, Clock, TrendingUp, Zap, ArrowRight, CornerDownLeft } from 'lucide-react';

export const SearchPage: React.FC = () => {
  const [query, setQuery] = useState('');
  const [searchMode, setSearchMode] = useState<'prefix' | 'forgotten'>('prefix');
  const [rankingMode, setRankingMode] = useState<'cached' | 'heap'>('cached');

  const [suggestions, setSuggestions] = useState<Suggestion[]>([]);
  const [conceptResults, setConceptResults] = useState<ConceptResult[]>([]);
  const [latencyUs, setLatencyUs] = useState<number | null>(null);
  const [popular, setPopular] = useState<PopularItem[]>([]);
  const [history, setHistory] = useState<HistoryItem[]>([]);
  const [selectedIndex, setSelectedIndex] = useState<number>(-1);
  const [lastSubmitted, setLastSubmitted] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  const dropdownRef = useRef<HTMLDivElement>(null);

  // Load initial popular searches and history
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
    try {
      const res = await api.forgottenSearch(query);
      setConceptResults(res.results || []);
    } catch (err) {
      console.error(err);
    } finally {
      setIsLoading(false);
    }
  };

  // Submit search (triggers frequency increment in Hash Table & persistence)
  const handleSubmitKeyword = async (keyword: string) => {
    if (!keyword.trim()) return;
    try {
      await api.submitSearch(keyword);
      setLastSubmitted(keyword);
      setQuery('');
      setSuggestions([]);
      setSelectedIndex(-1);
      // Refresh sidebar data
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
      setSelectedIndex((prev) => (prev < suggestions.length - 1 ? prev + 1 : 0));
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      setSelectedIndex((prev) => (prev > 0 ? prev - 1 : suggestions.length - 1));
    } else if (e.key === 'Enter') {
      e.preventDefault();
      const chosen = selectedIndex >= 0 ? suggestions[selectedIndex].keyword : query;
      handleSubmitKeyword(chosen);
    } else if (e.key === 'Escape') {
      setSuggestions([]);
      setSelectedIndex(-1);
    }
  };

  return (
    <div className="max-w-5xl mx-auto px-4 sm:px-6 py-8">
      {/* Header Banner */}
      <div className="text-center mb-8">
        <h1 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
          High-Performance Autocomplete
        </h1>
        <p className="mt-2 text-slate-400 text-sm sm:text-base max-w-2xl mx-auto">
          In-memory C engine utilizing a <span className="text-teal-400 font-mono">Trie</span> for $O(L)$ prefix search, a <span className="text-teal-400 font-mono">Hash Table</span> for frequency management, and a <span className="text-teal-400 font-mono">Min Heap</span> for Top-10 ranking.
        </p>
      </div>

      {/* Mode Controls Card */}
      <div className="flex flex-wrap items-center justify-between gap-4 bg-slate-900/60 border border-slate-800 rounded-xl p-3 mb-6">
        <div className="flex items-center space-x-2">
          <span className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Search Mode:</span>
          <button
            onClick={() => { setSearchMode('prefix'); setConceptResults([]); }}
            className={`px-3 py-1.5 rounded-lg text-xs font-medium transition-all ${
              searchMode === 'prefix'
                ? 'bg-teal-500 text-slate-950 font-semibold'
                : 'text-slate-400 hover:text-white bg-slate-800'
            }`}
          >
            Prefix Autocomplete
          </button>
          <button
            onClick={() => { setSearchMode('forgotten'); setSuggestions([]); }}
            className={`px-3 py-1.5 rounded-lg text-xs font-medium flex items-center space-x-1.5 transition-all ${
              searchMode === 'forgotten'
                ? 'bg-teal-500 text-slate-950 font-semibold'
                : 'text-slate-400 hover:text-white bg-slate-800'
            }`}
          >
            <Sparkles className="w-3.5 h-3.5" />
            <span>Forgotten-Name Search</span>
          </button>
        </div>

        {searchMode === 'prefix' && (
          <div className="flex items-center space-x-2">
            <span className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Algorithm:</span>
            <button
              onClick={() => setRankingMode('cached')}
              className={`px-2.5 py-1 rounded text-xs font-mono transition-all ${
                rankingMode === 'cached'
                  ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/40'
                  : 'text-slate-400 hover:text-white bg-slate-800/80'
              }`}
            >
              Approach B (Top-10 Cache)
            </button>
            <button
              onClick={() => setRankingMode('heap')}
              className={`px-2.5 py-1 rounded text-xs font-mono transition-all ${
                rankingMode === 'heap'
                  ? 'bg-sky-500/20 text-sky-300 border border-sky-500/40'
                  : 'text-slate-400 hover:text-white bg-slate-800/80'
              }`}
            >
              Approach A (Min Heap)
            </button>
          </div>
        )}
      </div>

      {/* Main Search Input */}
      <div className="relative mb-6">
        <div className="relative flex items-center">
          <div className="absolute left-4 pointer-events-none text-slate-400">
            {searchMode === 'prefix' ? <SearchIcon className="w-5 h-5" /> : <Sparkles className="w-5 h-5 text-teal-400" />}
          </div>
          <input
            type="text"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder={
              searchMode === 'prefix'
                ? "Type a prefix (e.g. 'pro', 'data', 'py')..."
                : "Describe what you remember (e.g. 'connect laptop to projector', 'store previous results')..."
            }
            className="w-full pl-12 pr-28 py-4 bg-slate-800/80 border border-slate-700/80 focus:border-teal-500 focus:ring-2 focus:ring-teal-500/20 rounded-2xl text-white placeholder-slate-500 text-base shadow-xl outline-none transition-all"
            autoFocus
          />
          <div className="absolute right-3 flex items-center space-x-2">
            {searchMode === 'forgotten' ? (
              <button
                onClick={handleForgottenSearch}
                disabled={isLoading}
                className="px-4 py-2 bg-teal-500 hover:bg-teal-400 text-slate-950 font-semibold text-xs rounded-xl transition-all shadow-md flex items-center space-x-1"
              >
                <span>Find Term</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </button>
            ) : (
              <div className="flex items-center space-x-1 text-xs text-slate-500 bg-slate-900/60 px-2.5 py-1 rounded-lg border border-slate-700/40">
                <span>Enter</span>
                <CornerDownLeft className="w-3 h-3" />
              </div>
            )}
          </div>
        </div>

        {/* Latency badge */}
        {latencyUs !== null && searchMode === 'prefix' && (
          <div className="mt-2 flex items-center justify-between text-xs text-slate-400 px-2 font-mono">
            <span className="flex items-center space-x-1">
              <Zap className="w-3.5 h-3.5 text-amber-400" />
              <span>Query latency: <strong className="text-white">{latencyUs.toFixed(1)} μs</strong></span>
            </span>
            <span className="text-slate-500">
              Mode: {rankingMode === 'cached' ? 'Trie Node Cache (O(L+K))' : 'Dynamic Min Heap (O(M log K))'}
            </span>
          </div>
        )}

        {/* Prefix Suggestions Dropdown */}
        {suggestions.length > 0 && searchMode === 'prefix' && (
          <div
            ref={dropdownRef}
            className="absolute left-0 right-0 mt-2 bg-slate-900 border border-slate-700/80 rounded-2xl shadow-2xl overflow-hidden z-40 backdrop-blur"
          >
            <div className="px-4 py-2 bg-slate-950/60 border-b border-slate-800 flex items-center justify-between text-xs text-slate-400 font-mono">
              <span>Top-{suggestions.length} Frequency-Ranked Suggestions</span>
              <span>Popularity</span>
            </div>
            <ul className="divide-y divide-slate-800/60">
              {suggestions.map((item, idx) => (
                <li
                  key={idx}
                  onClick={() => handleSubmitKeyword(item.keyword)}
                  className={`px-4 py-3 flex items-center justify-between cursor-pointer transition-colors ${
                    selectedIndex === idx ? 'bg-teal-500/10 border-l-4 border-teal-500' : 'hover:bg-slate-800/50'
                  }`}
                >
                  <div className="flex items-center space-x-3">
                    <span className="w-6 h-6 rounded-md bg-slate-800 text-slate-400 text-xs flex items-center justify-center font-mono font-bold">
                      #{idx + 1}
                    </span>
                    <span className="text-white font-medium">{item.keyword}</span>
                  </div>
                  <div className="flex items-center space-x-2">
                    <span className="text-xs px-2.5 py-0.5 rounded-full bg-slate-800 text-teal-400 font-mono font-semibold">
                      {item.frequency} searches
                    </span>
                  </div>
                </li>
              ))}
            </ul>
          </div>
        )}
      </div>

      {/* Submitted Banner Confirmation */}
      {lastSubmitted && (
        <div className="mb-6 p-4 rounded-xl bg-teal-500/10 border border-teal-500/30 flex items-center justify-between text-sm">
          <div className="flex items-center space-x-2 text-teal-300">
            <span>Search submitted for: <strong>"{lastSubmitted}"</strong>. Frequency dynamically updated in Hash Table and SQLite!</span>
          </div>
          <button
            onClick={() => setLastSubmitted(null)}
            className="text-xs text-slate-400 hover:text-white"
          >
            Dismiss
          </button>
        </div>
      )}

      {/* Forgotten-Name Results */}
      {searchMode === 'forgotten' && conceptResults.length > 0 && (
        <div className="space-y-4 mb-8">
          <h2 className="text-lg font-bold text-white flex items-center space-x-2">
            <Sparkles className="w-5 h-5 text-teal-400" />
            <span>Retrieved Concept Matches (Inverted Index Scoring)</span>
          </h2>
          <div className="grid grid-cols-1 gap-4">
            {conceptResults.map((c, i) => (
              <div key={i} className="bg-slate-800/40 border border-slate-700/80 rounded-xl p-5 hover:border-teal-500/50 transition-all">
                <div className="flex items-start justify-between">
                  <div>
                    <h3 className="text-lg font-bold text-teal-400">{c.concept_name}</h3>
                    <p className="mt-1 text-sm text-slate-300 leading-relaxed">{c.description}</p>
                  </div>
                  <div className="text-right">
                    <span className="px-2.5 py-1 rounded-full text-xs font-mono font-bold bg-teal-500/20 text-teal-300 border border-teal-500/30">
                      Score: {(c.relevance_score * 100).toFixed(0)}%
                    </span>
                  </div>
                </div>

                {c.related_keywords.length > 0 && (
                  <div className="mt-4 pt-3 border-t border-slate-700/50 flex flex-wrap items-center gap-1.5">
                    <span className="text-xs text-slate-500 mr-1 font-medium">Related terms:</span>
                    {c.related_keywords.map((kw, kidx) => (
                      <button
                        key={kidx}
                        onClick={() => {
                          setSearchMode('prefix');
                          setQuery(kw);
                        }}
                        className="text-xs bg-slate-900/80 hover:bg-teal-500/20 hover:text-teal-300 text-slate-300 px-2.5 py-1 rounded-md border border-slate-700/60 transition-all"
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
        {/* Popular searches */}
        <div className="bg-slate-900/40 border border-slate-800 rounded-xl p-5">
          <div className="flex items-center space-x-2 mb-4 text-slate-300">
            <TrendingUp className="w-4 h-4 text-teal-400" />
            <h3 className="font-semibold text-sm">Most Popular Searches</h3>
          </div>
          <div className="space-y-2">
            {popular.length === 0 ? (
              <p className="text-xs text-slate-500">No popular searches recorded yet.</p>
            ) : (
              popular.slice(0, 5).map((item) => (
                <div
                  key={item.id}
                  onClick={() => {
                    setSearchMode('prefix');
                    setQuery(item.keyword);
                  }}
                  className="flex items-center justify-between p-2.5 rounded-lg hover:bg-slate-800/60 cursor-pointer transition-colors text-sm"
                >
                  <span className="text-slate-200">{item.keyword}</span>
                  <span className="text-xs font-mono text-slate-400 bg-slate-800 px-2 py-0.5 rounded">
                    {item.frequency}
                  </span>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Recent history */}
        <div className="bg-slate-900/40 border border-slate-800 rounded-xl p-5">
          <div className="flex items-center space-x-2 mb-4 text-slate-300">
            <Clock className="w-4 h-4 text-teal-400" />
            <h3 className="font-semibold text-sm">Recent Search Activity</h3>
          </div>
          <div className="space-y-2">
            {history.length === 0 ? (
              <p className="text-xs text-slate-500">No recent searches yet. Submit a search to record history.</p>
            ) : (
              history.slice(0, 5).map((item) => (
                <div
                  key={item.id}
                  onClick={() => {
                    setSearchMode('prefix');
                    setQuery(item.keyword);
                  }}
                  className="flex items-center justify-between p-2.5 rounded-lg hover:bg-slate-800/60 cursor-pointer transition-colors text-sm"
                >
                  <span className="text-slate-200">{item.keyword}</span>
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

