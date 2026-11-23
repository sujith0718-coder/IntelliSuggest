import React, { useEffect, useState } from 'react';
import { api, KeywordRecord } from '../services/api';
import {
  ShieldCheck,
  Plus,
  Trash2,
  Clock,
  CheckCircle2,
  Search,
  Download,
  BookOpen
} from 'lucide-react';
import { sounds } from '../utils/sound';

export const AdminPage: React.FC = () => {
  const [keywords, setKeywords] = useState<KeywordRecord[]>([]);
  const [searchFilter, setSearchFilter] = useState('');
  const [newKeyword, setNewKeyword] = useState('');
  const [newFrequency, setNewFrequency] = useState(10);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [activeTab, setActiveTab] = useState<'keywords' | 'concepts'>('keywords');

  const fetchKeywords = async () => {
    try {
      const res = await api.getKeywords();
      setKeywords(res.keywords || []);
    } catch (err) {
      console.error(err);
    }
  };

  useEffect(() => {
    fetchKeywords();
  }, []);

  const handleAddKeyword = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newKeyword.trim()) return;

    setIsLoading(true);
    sounds.playSubmit();
    try {
      await api.addKeyword(newKeyword.trim(), Number(newFrequency));
      setStatusMessage(`Keyword "${newKeyword}" added to SQLite, Hash Table, and Trie Top-10 cache.`);
      setNewKeyword('');
      setNewFrequency(10);
      fetchKeywords();
    } catch {
      setStatusMessage('Error adding keyword');
    } finally {
      setIsLoading(false);
    }
  };

  const handleCleanup = async () => {
    if (!window.confirm('Run automated keyword cleanup for entries older than 90 days?')) return;
    setIsLoading(true);
    sounds.playSubmit();
    try {
      const res = await api.cleanupExpired();
      setStatusMessage(`Cleanup completed: ${res.cleaned_count} expired keywords purged.`);
      fetchKeywords();
    } catch {
      setStatusMessage('Error running cleanup');
    } finally {
      setIsLoading(false);
    }
  };

  // Export current keyword dataset to CSV
  const handleExportCSV = () => {
    sounds.playSelect();
    const csvContent =
      'data:text/csv;charset=utf-8,keyword,frequency,created_at,last_searched_at\n' +
      keywords.map((k) => `"${k.keyword}",${k.frequency},"${k.created_at}","${k.last_searched_at}"`).join('\n');
    const encodedUri = encodeURI(csvContent);
    const link = document.createElement('a');
    link.setAttribute('href', encodedUri);
    link.setAttribute('download', `intellisuggest_keywords_${Date.now()}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  const filteredKeywords = keywords.filter((k) =>
    k.keyword.toLowerCase().includes(searchFilter.trim().toLowerCase())
  );

  const defaultConcepts = [
    { name: 'Memoization', desc: 'Technique that stores previously computed results to avoid repeated calculations.', keywords: 'caching, dynamic programming, lookup table' },
    { name: 'Dijkstra Algorithm', desc: 'Algorithm that finds the shortest path between nodes in a weighted graph with non-negative edge weights.', keywords: 'shortest path, graph search, priority queue' },
    { name: 'HDMI Adapter', desc: 'Device used to connect a laptop or computer to a projector, monitor or external display.', keywords: 'usb-c to hdmi, display adapter, projector connector' },
    { name: 'Laptop Cooling Pad', desc: 'External accessory used to cool a laptop and prevent thermal throttling or overheating with fans.', keywords: 'cooling stand, laptop cooler, fan pad' },
    { name: 'Binary Search', desc: 'Search algorithm that finds the position of a target value within a sorted array by dividing interval in half.', keywords: 'divide and conquer, logarithmic search, sorted array' },
    { name: 'LRU Cache', desc: 'Caching data structure that evicts the least recently used elements first when capacity limit is reached.', keywords: 'least recently used, cache eviction, doubly linked list' },
    { name: 'Depth First Search', desc: 'Graph traversal algorithm exploring as deep as possible along each branch before backtracking.', keywords: 'dfs, tree traversal, recursion' },
    { name: 'Breadth First Search', desc: 'Graph traversal algorithm exploring all neighbor nodes at present depth before moving deeper.', keywords: 'bfs, level order traversal, queue' },
    { name: 'Hash Table', desc: 'Associative array mapping keys to values using a hash function and collision resolution.', keywords: 'hash map, dictionary, separate chaining' },
    { name: 'Min Heap', desc: 'Complete binary tree where parent node is always smaller than or equal to children.', keywords: 'priority queue, binary heap, top-k ranking' },
  ];

  return (
    <div className="max-w-6xl mx-auto px-4 sm:px-6 py-8">
      {/* Header */}
      <div className="flex flex-wrap items-center justify-between gap-4 mb-8">
        <div>
          <div className="inline-flex items-center space-x-2 px-2.5 py-0.5 rounded-full bg-slate-900 border border-slate-800 text-[11px] font-mono text-slate-400 mb-2">
            <span className="w-1.5 h-1.5 rounded-full bg-indigo-400" />
            <span>Administrative Controls & Catalog</span>
          </div>
          <h1 className="text-2xl sm:text-3xl font-extrabold text-white tracking-tight flex items-center space-x-3">
            <ShieldCheck className="w-7 h-7 text-teal-400" />
            <span>Keyword & Concept Management</span>
          </h1>
          <p className="text-slate-400 text-xs sm:text-sm mt-1">
            Manage persistent database entries, inspect concept inverted index, and trigger cache syncs.
          </p>
        </div>

        <div className="flex items-center space-x-2">
          <button
            onClick={handleExportCSV}
            className="flex items-center space-x-1.5 px-3.5 py-2 bg-slate-900 hover:bg-slate-800 text-slate-200 border border-slate-800 rounded-xl text-xs font-semibold transition-all shadow-sm"
          >
            <Download className="w-3.5 h-3.5 text-teal-400" />
            <span>Export CSV</span>
          </button>

          <button
            onClick={handleCleanup}
            disabled={isLoading}
            className="flex items-center space-x-1.5 px-3.5 py-2 bg-rose-500/10 hover:bg-rose-500/20 text-rose-400 border border-rose-500/30 rounded-xl text-xs font-semibold transition-all"
          >
            <Trash2 className="w-3.5 h-3.5" />
            <span>Cleanup Expired (&gt;90d)</span>
          </button>
        </div>
      </div>

      {statusMessage && (
        <div className="mb-6 p-4 rounded-xl bg-teal-500/10 border border-teal-500/30 flex items-center justify-between text-xs sm:text-sm text-teal-300">
          <div className="flex items-center space-x-2">
            <CheckCircle2 className="w-4 h-4 flex-shrink-0" />
            <span>{statusMessage}</span>
          </div>
          <button onClick={() => setStatusMessage(null)} className="text-xs text-slate-400 hover:text-white">
            Dismiss
          </button>
        </div>
      )}

      {/* Tabs */}
      <div className="flex items-center space-x-2 border-b border-slate-800/80 pb-3 mb-6 text-xs font-medium">
        <button
          onClick={() => {
            sounds.playSelect();
            setActiveTab('keywords');
          }}
          className={`flex items-center space-x-1.5 px-3.5 py-1.5 rounded-lg transition-all ${
            activeTab === 'keywords'
              ? 'bg-slate-800 text-white font-bold border border-slate-700'
              : 'text-slate-400 hover:text-white'
          }`}
        >
          <span>Keywords Directory ({keywords.length})</span>
        </button>

        <button
          onClick={() => {
            sounds.playSelect();
            setActiveTab('concepts');
          }}
          className={`flex items-center space-x-1.5 px-3.5 py-1.5 rounded-lg transition-all ${
            activeTab === 'concepts'
              ? 'bg-slate-800 text-white font-bold border border-slate-700'
              : 'text-slate-400 hover:text-white'
          }`}
        >
          <BookOpen className="w-3.5 h-3.5 text-teal-400" />
          <span>Forgotten-Name Concepts ({defaultConcepts.length})</span>
        </button>
      </div>

      {activeTab === 'keywords' && (
        <>
          {/* Add Keyword Form */}
          <div className="bg-slate-900/60 border border-slate-800 rounded-2xl p-6 mb-8 shadow-xl">
            <h2 className="text-xs font-bold text-slate-300 uppercase tracking-wider mb-4 flex items-center space-x-2">
              <Plus className="w-4 h-4 text-teal-400" />
              <span>Insert Keyword into SQLite & Trie</span>
            </h2>
            <form onSubmit={handleAddKeyword} className="flex flex-wrap items-center gap-3">
              <div className="flex-1 min-w-[220px]">
                <input
                  type="text"
                  value={newKeyword}
                  onChange={(e) => setNewKeyword(e.target.value)}
                  placeholder="Keyword (e.g. 'convolutional neural network')"
                  className="w-full px-4 py-2.5 bg-slate-950 border border-slate-800 rounded-xl text-white placeholder-slate-500 text-xs focus:border-teal-500 outline-none"
                  required
                />
              </div>
              <div className="w-36">
                <input
                  type="number"
                  value={newFrequency}
                  onChange={(e) => setNewFrequency(Number(e.target.value))}
                  placeholder="Frequency"
                  min="1"
                  className="w-full px-4 py-2.5 bg-slate-950 border border-slate-800 rounded-xl text-white placeholder-slate-500 text-xs focus:border-teal-500 outline-none font-mono"
                  required
                />
              </div>
              <button
                type="submit"
                disabled={isLoading}
                className="px-5 py-2.5 bg-teal-500 hover:bg-teal-400 text-slate-950 font-bold text-xs rounded-xl transition-all shadow-md"
              >
                Add Keyword
              </button>
            </form>
          </div>

          {/* Search Filter and Table */}
          <div className="bg-slate-900/50 border border-slate-800 rounded-2xl overflow-hidden shadow-xl">
            <div className="px-6 py-4 border-b border-slate-800 flex flex-wrap items-center justify-between gap-3">
              <h2 className="text-xs font-bold text-slate-300 uppercase tracking-wider">
                Active Keywords Directory
              </h2>
              <div className="relative w-64">
                <Search className="w-3.5 h-3.5 absolute left-3 top-2.5 text-slate-500" />
                <input
                  type="text"
                  value={searchFilter}
                  onChange={(e) => setSearchFilter(e.target.value)}
                  placeholder="Filter keywords..."
                  className="w-full pl-8 pr-3 py-1.5 bg-slate-950 border border-slate-800 rounded-lg text-xs text-white placeholder-slate-500 outline-none focus:border-teal-500"
                />
              </div>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-left text-xs text-slate-400">
                <thead className="bg-slate-950/80 text-[11px] text-slate-400 uppercase font-mono border-b border-slate-800">
                  <tr>
                    <th className="px-6 py-3">ID</th>
                    <th className="px-6 py-3">Keyword</th>
                    <th className="px-6 py-3">Frequency</th>
                    <th className="px-6 py-3">Created At</th>
                    <th className="px-6 py-3">Last Searched</th>
                    <th className="px-6 py-3">Status</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-800/60 font-sans">
                  {filteredKeywords.map((kw) => (
                    <tr key={kw.id} className="hover:bg-slate-800/40 transition-colors">
                      <td className="px-6 py-3 font-mono text-slate-500">{kw.id}</td>
                      <td className="px-6 py-3 font-semibold text-white">{kw.keyword}</td>
                      <td className="px-6 py-3 font-mono text-teal-400 font-bold">{kw.frequency}</td>
                      <td className="px-6 py-3 text-slate-400">{new Date(kw.created_at).toLocaleDateString()}</td>
                      <td className="px-6 py-3">
                        {kw.last_searched_at ? (
                          <span className="flex items-center space-x-1 text-slate-300">
                            <Clock className="w-3 h-3 text-slate-500" />
                            <span>{new Date(kw.last_searched_at).toLocaleDateString()}</span>
                          </span>
                        ) : (
                          <span className="text-slate-600">Never</span>
                        )}
                      </td>
                      <td className="px-6 py-3">
                        <span
                          className={`text-[10px] font-mono px-2 py-0.5 rounded-full ${
                            kw.is_active
                              ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20'
                              : 'bg-rose-500/10 text-rose-400 border border-rose-500/20'
                          }`}
                        >
                          {kw.is_active ? 'Active' : 'Inactive'}
                        </span>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </>
      )}

      {activeTab === 'concepts' && (
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {defaultConcepts.map((c, i) => (
            <div key={i} className="bg-slate-900/60 border border-slate-800 rounded-2xl p-5 shadow-lg">
              <div className="flex items-center justify-between mb-2">
                <h3 className="text-sm font-bold text-teal-400">{c.name}</h3>
                <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-slate-950 text-slate-400 border border-slate-800">
                  Concept #{i + 1}
                </span>
              </div>
              <p className="text-xs text-slate-300 leading-relaxed mb-3">{c.desc}</p>
              <div className="pt-2.5 border-t border-slate-800/80 text-[11px] text-slate-400">
                <span className="text-slate-500 font-medium">Index Synonyms:</span>{' '}
                <span className="font-mono text-slate-300">{c.keywords}</span>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};
