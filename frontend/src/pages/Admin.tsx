import React, { useEffect, useState } from 'react';
import { api, KeywordRecord } from '../services/api';
import { ShieldCheck, Plus, Trash2, Clock, CheckCircle2 } from 'lucide-react';

export const AdminPage: React.FC = () => {
  const [keywords, setKeywords] = useState<KeywordRecord[]>([]);
  const [newKeyword, setNewKeyword] = useState('');
  const [newFrequency, setNewFrequency] = useState(10);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(false);

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
    try {
      await api.addKeyword(newKeyword.trim(), Number(newFrequency));
      setStatusMessage(`Keyword "${newKeyword}" successfully added to SQLite, Hash Table, and Trie.`);
      setNewKeyword('');
      setNewFrequency(10);
      fetchKeywords();
    } catch (err) {
      setStatusMessage('Error adding keyword');
    } finally {
      setIsLoading(false);
    }
  };

  const handleCleanup = async () => {
    if (!window.confirm('Run automated keyword cleanup for entries older than 90 days?')) return;
    setIsLoading(true);
    try {
      const res = await api.cleanupExpired();
      setStatusMessage(`Cleanup completed: ${res.cleaned_count} expired keywords removed.`);
      fetchKeywords();
    } catch (err) {
      setStatusMessage('Error running cleanup');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="max-w-6xl mx-auto px-4 sm:px-6 py-8">
      <div className="flex items-center justify-between mb-8">
        <div>
          <h1 className="text-2xl sm:text-3xl font-extrabold text-white flex items-center space-x-3">
            <ShieldCheck className="w-8 h-8 text-teal-400" />
            <span>Keyword Administration & Maintenance</span>
          </h1>
          <p className="text-slate-400 text-sm mt-1">Manage database records and trigger memory/cache synchronizations</p>
        </div>
        <button
          onClick={handleCleanup}
          disabled={isLoading}
          className="flex items-center space-x-2 px-4 py-2 bg-rose-500/10 hover:bg-rose-500/20 text-rose-400 border border-rose-500/30 rounded-xl text-xs font-semibold transition-all"
        >
          <Trash2 className="w-3.5 h-3.5" />
          <span>Cleanup Expired (&gt;90d)</span>
        </button>
      </div>

      {statusMessage && (
        <div className="mb-6 p-4 rounded-xl bg-teal-500/10 border border-teal-500/30 flex items-center justify-between text-sm text-teal-300">
          <div className="flex items-center space-x-2">
            <CheckCircle2 className="w-4 h-4" />
            <span>{statusMessage}</span>
          </div>
          <button onClick={() => setStatusMessage(null)} className="text-xs text-slate-400 hover:text-white">
            Dismiss
          </button>
        </div>
      )}

      {/* Add Keyword Form */}
      <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-6 mb-8">
        <h2 className="text-sm font-bold text-slate-300 uppercase tracking-wider mb-4 flex items-center space-x-2">
          <Plus className="w-4 h-4 text-teal-400" />
          <span>Insert New Keyword</span>
        </h2>
        <form onSubmit={handleAddKeyword} className="flex flex-wrap items-center gap-4">
          <div className="flex-1 min-w-[200px]">
            <input
              type="text"
              value={newKeyword}
              onChange={(e) => setNewKeyword(e.target.value)}
              placeholder="Keyword (e.g. 'convolutional neural network')"
              className="w-full px-4 py-2.5 bg-slate-800 border border-slate-700 rounded-xl text-white placeholder-slate-500 text-sm focus:border-teal-500 outline-none"
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
              className="w-full px-4 py-2.5 bg-slate-800 border border-slate-700 rounded-xl text-white placeholder-slate-500 text-sm focus:border-teal-500 outline-none font-mono"
              required
            />
          </div>
          <button
            type="submit"
            disabled={isLoading}
            className="px-5 py-2.5 bg-teal-500 hover:bg-teal-400 text-slate-950 font-bold text-sm rounded-xl transition-all shadow-md"
          >
            Add Keyword
          </button>
        </form>
      </div>

      {/* Keywords Table */}
      <div className="bg-slate-900/50 border border-slate-800 rounded-xl overflow-hidden shadow-xl">
        <div className="px-6 py-4 border-b border-slate-800 flex items-center justify-between">
          <h2 className="text-sm font-bold text-slate-300 uppercase tracking-wider">
            Indexed Keywords Directory ({keywords.length})
          </h2>
        </div>

        <div className="overflow-x-auto">
          <table className="w-full text-left text-sm text-slate-400">
            <thead className="bg-slate-950/60 text-xs text-slate-400 uppercase font-mono border-b border-slate-800">
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
              {keywords.map((kw) => (
                <tr key={kw.id} className="hover:bg-slate-800/40 transition-colors">
                  <td className="px-6 py-3 font-mono text-xs">{kw.id}</td>
                  <td className="px-6 py-3 font-medium text-white">{kw.keyword}</td>
                  <td className="px-6 py-3 font-mono text-teal-400 font-semibold">{kw.frequency}</td>
                  <td className="px-6 py-3 text-xs">{new Date(kw.created_at).toLocaleDateString()}</td>
                  <td className="px-6 py-3 text-xs">
                    {kw.last_searched_at ? (
                      <span className="flex items-center space-x-1">
                        <Clock className="w-3 h-3 text-slate-500" />
                        <span>{new Date(kw.last_searched_at).toLocaleDateString()}</span>
                      </span>
                    ) : (
                      <span className="text-slate-600">Never</span>
                    )}
                  </td>
                  <td className="px-6 py-3">
                    <span
                      className={`text-xs px-2 py-0.5 rounded-full ${
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
    </div>
  );
};

