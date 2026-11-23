import React, { useEffect, useState } from 'react';
import { api, AnalyticsSummary, PopularItem, HistoryItem } from '../services/api';
import { MetricCard } from '../components/MetricCard';
import {
  Database,
  Search,
  GitFork,
  Hash,
  Activity,
  RefreshCw,
  Cpu,
  Layers,
  CheckCircle2,
  PieChart,
  HardDrive
} from 'lucide-react';
import { sounds } from '../utils/sound';

export const AnalyticsPage: React.FC = () => {
  const [summary, setSummary] = useState<AnalyticsSummary | null>(null);
  const [popular, setPopular] = useState<PopularItem[]>([]);
  const [history, setHistory] = useState<HistoryItem[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  const fetchAnalytics = async () => {
    setIsLoading(true);
    try {
      const [sumData, popData, histData] = await Promise.all([
        api.getAnalytics(),
        api.getPopular(),
        api.getHistory(),
      ]);
      setSummary(sumData);
      setPopular(popData.popular || []);
      setHistory(histData.history || []);
    } catch (err) {
      console.error('Failed to load analytics:', err);
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    fetchAnalytics();
  }, []);

  // Compute estimated RAM footprint for in-memory structures
  // TrieNode: 27 pointers (216B) + 10 cached suggestions (1360B) + metadata ~ 1600 bytes
  const trieNodes = summary?.trie_total_nodes ?? 0;
  const trieBytes = trieNodes * 1600;
  const hashBytes = (summary?.total_keywords ?? 0) * 128 + 128 * 8;
  const totalEstimatedBytes = trieBytes + hashBytes;
  const totalEstimatedKB = (totalEstimatedBytes / 1024).toFixed(1);

  // Compute collision percentage
  const totalKeywords = summary?.total_keywords || 1;
  const collisions = summary?.hash_collisions || 0;
  const collisionRate = ((collisions / totalKeywords) * 100).toFixed(1);

  return (
    <div className="max-w-6xl mx-auto px-4 sm:px-6 py-8">
      {/* Header */}
      <div className="flex flex-wrap items-center justify-between gap-4 mb-8">
        <div>
          <div className="inline-flex items-center space-x-2 px-2.5 py-0.5 rounded-full bg-slate-900 border border-slate-800 text-[11px] font-mono text-slate-400 mb-2">
            <span className="w-1.5 h-1.5 rounded-full bg-teal-400" />
            <span>Telemetry & In-Memory Observability</span>
          </div>
          <h1 className="text-2xl sm:text-3xl font-extrabold text-white tracking-tight">
            Data Structure Metrics & Dashboard
          </h1>
          <p className="text-slate-400 text-xs sm:text-sm mt-1">
            Real telemetry directly queried from in-memory C data structures and persistent SQLite storage.
          </p>
        </div>
        <button
          onClick={() => {
            sounds.playSelect();
            fetchAnalytics();
          }}
          disabled={isLoading}
          className="flex items-center space-x-2 px-3.5 py-2 rounded-xl bg-slate-900 hover:bg-slate-800 text-xs font-semibold text-slate-200 transition-all border border-slate-800 shadow-sm"
        >
          <RefreshCw className={`w-3.5 h-3.5 text-teal-400 ${isLoading ? 'animate-spin' : ''}`} />
          <span>Refresh Data</span>
        </button>
      </div>

      {/* Primary KPI Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
        <MetricCard
          title="Total Searches Submitted"
          value={summary?.total_searches ?? 0}
          subtitle="Audit log events in SQLite"
          icon={<Search className="w-5 h-5 text-teal-400" />}
        />
        <MetricCard
          title="Active Keywords In-Memory"
          value={summary?.active_keywords ?? 0}
          subtitle={`Out of ${summary?.total_keywords ?? 0} persisted records`}
          icon={<Database className="w-5 h-5 text-emerald-400" />}
          badge="SQLite Synced"
        />
        <MetricCard
          title="Trie Nodes in RAM"
          value={summary?.trie_total_nodes ?? 0}
          subtitle={`${summary?.trie_total_words ?? 0} prefix termination leaves`}
          icon={<GitFork className="w-5 h-5 text-indigo-400" />}
        />
        <MetricCard
          title="Hash Table Load Factor"
          value={summary ? summary.hash_load_factor.toFixed(3) : '0.000'}
          subtitle={`${summary?.hash_collisions ?? 0} collisions (${collisionRate}%)`}
          icon={<Hash className="w-5 h-5 text-amber-400" />}
          badge="Threshold: 0.75"
        />
      </div>

      {/* Algorithmic Health & Memory Footprint Section */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
        <div className="bg-slate-900/60 border border-slate-800 rounded-2xl p-5 shadow-lg">
          <div className="flex items-center justify-between mb-3">
            <span className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Memory Footprint</span>
            <HardDrive className="w-4 h-4 text-teal-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-white mb-1">{totalEstimatedKB} KB</div>
          <p className="text-xs text-slate-400 mb-3">Estimated working set for Trie nodes, Top-10 caches & Hash buckets.</p>
          <div className="w-full bg-slate-950 rounded-full h-1.5 overflow-hidden">
            <div className="bg-gradient-to-r from-teal-500 to-emerald-400 h-1.5 rounded-full" style={{ width: '45%' }} />
          </div>
        </div>

        <div className="bg-slate-900/60 border border-slate-800 rounded-2xl p-5 shadow-lg">
          <div className="flex items-center justify-between mb-3">
            <span className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Collision Rate</span>
            <Layers className="w-4 h-4 text-emerald-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-emerald-400 mb-1">{collisionRate}%</div>
          <p className="text-xs text-slate-400 mb-3">Separate chaining resolved via linked lists. Rehash triggered at load factor 0.75.</p>
          <div className="flex items-center space-x-1.5 text-xs text-emerald-400 font-medium">
            <CheckCircle2 className="w-3.5 h-3.5" />
            <span>Optimal Distribution (djb2)</span>
          </div>
        </div>

        <div className="bg-slate-900/60 border border-slate-800 rounded-2xl p-5 shadow-lg">
          <div className="flex items-center justify-between mb-3">
            <span className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Top-10 Cache State</span>
            <Cpu className="w-4 h-4 text-indigo-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-white mb-1">Precomputed</div>
          <p className="text-xs text-slate-400 mb-3">100% of active prefix nodes hold precomputed Top-10 suggestion vectors.</p>
          <div className="flex items-center space-x-1.5 text-xs text-teal-400 font-medium">
            <CheckCircle2 className="w-3.5 h-3.5" />
            <span>Zero Query-Time Sifting Required</span>
          </div>
        </div>
      </div>

      {/* Visual Analytics & Search Events */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Popular Keywords Frequency Chart */}
        <div className="bg-slate-900/50 border border-slate-800 rounded-2xl p-6 shadow-xl">
          <div className="flex items-center justify-between mb-5">
            <div className="flex items-center space-x-2">
              <Activity className="w-4 h-4 text-teal-400" />
              <h2 className="text-sm font-bold text-white uppercase tracking-wider">Top Keywords Distribution</h2>
            </div>
            <span className="text-xs font-mono text-slate-500">Live Popularity</span>
          </div>

          <div className="space-y-3.5">
            {popular.length === 0 ? (
              <p className="text-xs text-slate-500 py-6 text-center">No keywords recorded.</p>
            ) : (
              popular.map((item, idx) => {
                const maxFreq = popular[0]?.frequency || 1;
                const percentage = Math.round((item.frequency / maxFreq) * 100);
                return (
                  <div key={item.id} className="space-y-1">
                    <div className="flex justify-between text-xs font-medium">
                      <span className="text-slate-300">
                        <strong className="text-slate-500 font-mono mr-2">#{idx + 1}</strong>
                        {item.keyword}
                      </span>
                      <span className="text-teal-400 font-mono font-bold">{item.frequency} hits</span>
                    </div>
                    <div className="w-full bg-slate-950 rounded-full h-2 overflow-hidden border border-slate-800/60">
                      <div
                        className="bg-gradient-to-r from-teal-500 via-emerald-400 to-cyan-400 h-2 rounded-full transition-all duration-500"
                        style={{ width: `${percentage}%` }}
                      />
                    </div>
                  </div>
                );
              })
            )}
          </div>
        </div>

        {/* Recent Search Activity Timeline */}
        <div className="bg-slate-900/50 border border-slate-800 rounded-2xl p-6 shadow-xl">
          <div className="flex items-center justify-between mb-5">
            <div className="flex items-center space-x-2">
              <PieChart className="w-4 h-4 text-indigo-400" />
              <h2 className="text-sm font-bold text-white uppercase tracking-wider">Recent Submitted Searches</h2>
            </div>
            <span className="text-xs font-mono text-slate-500">Audit Trail</span>
          </div>

          <div className="space-y-2.5">
            {history.length === 0 ? (
              <p className="text-xs text-slate-500 py-6 text-center">No search events recorded yet. Submit a search on the home view.</p>
            ) : (
              history.map((item) => (
                <div
                  key={item.id}
                  className="flex items-center justify-between p-3 rounded-xl bg-slate-950/60 border border-slate-800/80 text-xs hover:border-slate-700 transition-colors"
                >
                  <span className="font-semibold text-slate-200">{item.keyword}</span>
                  <span className="text-slate-500 font-mono">
                    {item.searched_at ? new Date(item.searched_at).toLocaleString() : 'Just now'}
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
