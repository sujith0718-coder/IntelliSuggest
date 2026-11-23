import React, { useEffect, useState } from 'react';
import { api, KeywordRecord, AdminConceptRecord, setAdminToken, getAdminToken } from '../services/api';
import {
  ShieldCheck,
  Plus,
  Trash2,
  Clock,
  CheckCircle2,
  AlertCircle,
  Search,
  Download,
  BookOpen,
  Key,
  Unlock,
  Lock,
  RefreshCw
} from 'lucide-react';
import { sounds } from '../utils/sound';

export const AdminPage: React.FC = () => {
  const [keywords, setKeywords] = useState<KeywordRecord[]>([]);
  const [concepts, setConcepts] = useState<AdminConceptRecord[]>([]);
  const [searchFilter, setSearchFilter] = useState('');
  const [newKeyword, setNewKeyword] = useState('');
  const [newFrequency, setNewFrequency] = useState(10);
  const [statusMessage, setStatusMessage] = useState<{ text: string; isError?: boolean } | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [activeTab, setActiveTab] = useState<'keywords' | 'concepts'>('keywords');

  // Admin Token State
  const [adminKeyInput, setAdminKeyInput] = useState(() => getAdminToken() || '');
  const [hasSavedKey, setHasSavedKey] = useState(() => !!getAdminToken());

  // New Concept Form State
  const [newConceptName, setNewConceptName] = useState('');
  const [newConceptDesc, setNewConceptDesc] = useState('');
  const [newConceptKeywords, setNewConceptKeywords] = useState('');

  const fetchKeywords = async () => {
    try {
      const res = await api.getKeywords();
      setKeywords(res.keywords || []);
    } catch (err) {
      console.error('Failed to load keywords:', err);
    }
  };

  const fetchConcepts = async () => {
    try {
      const res = await api.getConcepts();
      setConcepts(res.concepts || []);
    } catch (err) {
      console.error('Failed to load concepts:', err);
    }
  };

  useEffect(() => {
    fetchKeywords();
    fetchConcepts();
  }, []);

  const handleSaveAdminKey = (e: React.FormEvent) => {
    e.preventDefault();
    const trimmed = adminKeyInput.trim();
    if (trimmed) {
      setAdminToken(trimmed);
      setHasSavedKey(true);
      setStatusMessage({ text: 'Admin API Key saved for this session.' });
      sounds.playSubmit();
    } else {
      setAdminToken(null);
      setHasSavedKey(false);
      setStatusMessage({ text: 'Admin API Key cleared.' });
    }
  };

  const handleClearAdminKey = () => {
    setAdminToken(null);
    setAdminKeyInput('');
    setHasSavedKey(false);
    setStatusMessage({ text: 'Admin API Key cleared from session.' });
    sounds.playSelect();
  };

  const handleAddKeyword = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newKeyword.trim()) return;

    setIsLoading(true);
    sounds.playSubmit();
    try {
      await api.addKeyword(newKeyword.trim(), Number(newFrequency));
      setStatusMessage({
        text: `Keyword "${newKeyword}" added to SQLite, Hash Table, and Trie Top-10 cache.`
      });
      setNewKeyword('');
      setNewFrequency(10);
      fetchKeywords();
    } catch (err: any) {
      setStatusMessage({ text: err.message || 'Error adding keyword', isError: true });
    } finally {
      setIsLoading(false);
    }
  };

  const handleDeleteKeyword = async (id: number, kw: string) => {
    if (!window.confirm(`Permanently remove keyword "${kw}" from database and memory?`)) return;
    setIsLoading(true);
    sounds.playSelect();
    try {
      await api.deleteKeyword(id);
      setStatusMessage({ text: `Keyword "${kw}" deleted successfully.` });
      fetchKeywords();
    } catch (err: any) {
      setStatusMessage({ text: err.message || 'Error deleting keyword', isError: true });
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
      setStatusMessage({ text: `Cleanup completed: ${res.cleaned_count} expired keywords purged.` });
      fetchKeywords();
    } catch (err: any) {
      setStatusMessage({ text: err.message || 'Error running cleanup', isError: true });
    } finally {
      setIsLoading(false);
    }
  };

  const handleAddConcept = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newConceptName.trim() || !newConceptDesc.trim()) return;

    setIsLoading(true);
    sounds.playSubmit();
    try {
      await api.addConcept(newConceptName.trim(), newConceptDesc.trim(), newConceptKeywords.trim());
      setStatusMessage({
        text: `Concept "${newConceptName}" added and inverted index updated in-memory.`
      });
      setNewConceptName('');
      setNewConceptDesc('');
      setNewConceptKeywords('');
      fetchConcepts();
    } catch (err: any) {
      setStatusMessage({ text: err.message || 'Error adding concept', isError: true });
    } finally {
      setIsLoading(false);
    }
  };

  const handleDeleteConcept = async (id: number, name: string) => {
    if (!window.confirm(`Delete concept "${name}" and re-index inverted postings in memory?`)) return;
    setIsLoading(true);
    sounds.playSelect();
    try {
      await api.deleteConcept(id);
      setStatusMessage({ text: `Concept "${name}" deleted and index rebalanced.` });
      fetchConcepts();
    } catch (err: any) {
      setStatusMessage({ text: err.message || 'Error deleting concept', isError: true });
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
            Manage persistent database entries, administer concept inverted index, and trigger cache updates.
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

      {/* Admin Authentication Key Card */}
      <div className="bg-slate-900/60 border border-slate-800/80 rounded-2xl p-4 sm:p-5 mb-6 shadow-md">
        <div className="flex flex-wrap items-center justify-between gap-3 mb-3">
          <div className="flex items-center space-x-2">
            <Key className="w-4 h-4 text-amber-400" />
            <h2 className="text-xs font-bold text-slate-200 uppercase tracking-wider">
              Admin Authentication
            </h2>
          </div>
          <span
            className={`inline-flex items-center space-x-1.5 text-[11px] font-mono px-2.5 py-0.5 rounded-full border ${
              hasSavedKey
                ? 'bg-emerald-500/10 text-emerald-400 border-emerald-500/30'
                : 'bg-amber-500/10 text-amber-400 border-amber-500/30'
            }`}
          >
            {hasSavedKey ? <Unlock className="w-3 h-3" /> : <Lock className="w-3 h-3" />}
            <span>{hasSavedKey ? 'Token Active' : 'Unauthenticated'}</span>
          </span>
        </div>
        <p className="text-xs text-slate-400 mb-3 leading-relaxed">
          Modifying operations (Add/Delete Keyword, Run Cleanup, and Dynamic Concept CRUD) require an{' '}
          <code className="text-teal-400 bg-slate-950 px-1 py-0.5 rounded text-[11px]">ADMIN_API_KEY</code>{' '}
          token matching your backend environment configuration.
        </p>
        <form onSubmit={handleSaveAdminKey} className="flex flex-wrap items-center gap-2">
          <div className="flex-1 min-w-[240px]">
            <input
              type="password"
              value={adminKeyInput}
              onChange={(e) => setAdminKeyInput(e.target.value)}
              placeholder="Enter ADMIN_API_KEY token..."
              className="w-full px-3.5 py-2 bg-slate-950 border border-slate-800 rounded-xl text-xs text-white placeholder-slate-600 focus:border-amber-400 outline-none font-mono"
            />
          </div>
          <button
            type="submit"
            className="px-4 py-2 bg-amber-500 hover:bg-amber-400 text-slate-950 font-bold text-xs rounded-xl transition-all shadow-sm"
          >
            Save Token
          </button>
          {hasSavedKey && (
            <button
              type="button"
              onClick={handleClearAdminKey}
              className="px-3.5 py-2 bg-slate-800 hover:bg-slate-700 text-slate-300 font-semibold text-xs rounded-xl transition-all border border-slate-700"
            >
              Clear
            </button>
          )}
        </form>
      </div>

      {statusMessage && (
        <div
          className={`mb-6 p-4 rounded-xl border flex items-center justify-between text-xs sm:text-sm ${
            statusMessage.isError
              ? 'bg-rose-500/10 border-rose-500/30 text-rose-300'
              : 'bg-teal-500/10 border-teal-500/30 text-teal-300'
          }`}
        >
          <div className="flex items-center space-x-2">
            {statusMessage.isError ? (
              <AlertCircle className="w-4 h-4 flex-shrink-0" />
            ) : (
              <CheckCircle2 className="w-4 h-4 flex-shrink-0" />
            )}
            <span>{statusMessage.text}</span>
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
          <span>Dynamic Forgotten-Name Concepts ({concepts.length})</span>
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
                    <th className="px-6 py-3 text-right">Actions</th>
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
                      <td className="px-6 py-3 text-right">
                        <button
                          onClick={() => handleDeleteKeyword(kw.id, kw.keyword)}
                          disabled={isLoading}
                          title="Delete keyword"
                          className="p-1.5 rounded-lg hover:bg-rose-500/10 text-slate-500 hover:text-rose-400 transition-colors"
                        >
                          <Trash2 className="w-4 h-4" />
                        </button>
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
        <div className="space-y-6">
          {/* Add Concept Form */}
          <div className="bg-slate-900/60 border border-slate-800 rounded-2xl p-6 shadow-xl">
            <h2 className="text-xs font-bold text-slate-300 uppercase tracking-wider mb-4 flex items-center space-x-2">
              <Plus className="w-4 h-4 text-teal-400" />
              <span>Add Dynamic Concept to Inverted Index</span>
            </h2>
            <form onSubmit={handleAddConcept} className="space-y-3">
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                <div>
                  <label className="block text-[11px] font-medium text-slate-400 mb-1">Concept Name</label>
                  <input
                    type="text"
                    value={newConceptName}
                    onChange={(e) => setNewConceptName(e.target.value)}
                    placeholder="e.g. 'A* Search Algorithm'"
                    className="w-full px-3.5 py-2 bg-slate-950 border border-slate-800 rounded-xl text-white placeholder-slate-600 text-xs focus:border-teal-500 outline-none"
                    required
                  />
                </div>
                <div>
                  <label className="block text-[11px] font-medium text-slate-400 mb-1">
                    Associated Keywords (comma-separated)
                  </label>
                  <input
                    type="text"
                    value={newConceptKeywords}
                    onChange={(e) => setNewConceptKeywords(e.target.value)}
                    placeholder="e.g. 'heuristic search, graph pathfinding, shortest path'"
                    className="w-full px-3.5 py-2 bg-slate-950 border border-slate-800 rounded-xl text-white placeholder-slate-600 text-xs focus:border-teal-500 outline-none"
                  />
                </div>
              </div>
              <div>
                <label className="block text-[11px] font-medium text-slate-400 mb-1">Concept Description</label>
                <textarea
                  value={newConceptDesc}
                  onChange={(e) => setNewConceptDesc(e.target.value)}
                  placeholder="Detailed description of the concept used for natural language inverted index matching..."
                  rows={2}
                  className="w-full px-3.5 py-2 bg-slate-950 border border-slate-800 rounded-xl text-white placeholder-slate-600 text-xs focus:border-teal-500 outline-none resize-none"
                  required
                />
              </div>
              <div className="flex justify-end">
                <button
                  type="submit"
                  disabled={isLoading}
                  className="px-5 py-2.5 bg-teal-500 hover:bg-teal-400 text-slate-950 font-bold text-xs rounded-xl transition-all shadow-md"
                >
                  Create Concept
                </button>
              </div>
            </form>
          </div>

          {/* Concepts Grid */}
          <div className="flex items-center justify-between">
            <h2 className="text-xs font-bold text-slate-300 uppercase tracking-wider">
              Registered Concepts ({concepts.length})
            </h2>
            <button
              onClick={() => {
                sounds.playSelect();
                fetchConcepts();
              }}
              className="flex items-center space-x-1 text-xs text-slate-400 hover:text-white"
            >
              <RefreshCw className="w-3 h-3 text-teal-400" />
              <span>Refresh Concepts</span>
            </button>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {concepts.map((c) => (
              <div key={c.id} className="bg-slate-900/60 border border-slate-800 rounded-2xl p-5 shadow-lg flex flex-col justify-between">
                <div>
                  <div className="flex items-center justify-between mb-2">
                    <h3 className="text-sm font-bold text-teal-400">{c.concept_name}</h3>
                    <div className="flex items-center space-x-2">
                      <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-slate-950 text-slate-400 border border-slate-800">
                        ID #{c.id}
                      </span>
                      <button
                        onClick={() => handleDeleteConcept(c.id, c.concept_name)}
                        disabled={isLoading}
                        title="Delete Concept"
                        className="p-1 rounded hover:bg-rose-500/10 text-slate-500 hover:text-rose-400 transition-colors"
                      >
                        <Trash2 className="w-3.5 h-3.5" />
                      </button>
                    </div>
                  </div>
                  <p className="text-xs text-slate-300 leading-relaxed mb-3">{c.description}</p>
                </div>
                {c.keywords && (
                  <div className="pt-2.5 border-t border-slate-800/80 text-[11px] text-slate-400">
                    <span className="text-slate-500 font-medium">Index Synonyms:</span>{' '}
                    <span className="font-mono text-slate-300">{c.keywords}</span>
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
