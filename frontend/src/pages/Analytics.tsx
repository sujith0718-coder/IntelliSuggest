import React, { useEffect, useState } from 'react';
import { api, AnalyticsSummary, PopularItem, HistoryItem } from '../services/api';
import { MetricCard } from '../components/MetricCard';
import { Database, Search, GitFork, Hash, Activity, RefreshCw } from 'lucide-react';

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

  return (
    <div className="max-w-6xl mx-auto px-4 sm:px-6 py-8">
      <div className="flex items-center justify-between mb-8">
        <div>
          <h1 className="text-2xl sm:text-3xl font-extrabold text-white">System Analytics & Data Structures</h1>
          <p className="text-slate-400 text-sm mt-1">Live metrics from in-memory C structures and SQLite persistence</p>
        </div>
        <button
          onClick={fetchAnalytics}
          disabled={isLoading}
          className="flex items-center space-x-2 px-3 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-medium text-slate-300 transition-all border border-slate-700"
        >
          <RefreshCw className={`w-3.5 h-3.5 ${isLoading ? 'animate-spin' : ''}`} />
          <span>Refresh</span>
        </button>
      </div>

      {/* Top Metric Cards */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
        <MetricCard
          title="Total Submitted Searches"
          value={summary?.total_searches ?? 0}
          subtitle="Persistent search events"
          icon={<Search className="w-5 h-5 text-teal-400" />}
        />
        <MetricCard
          title="Active Keywords"
          value={summary?.active_keywords ?? 0}
          subtitle={`Out of ${summary?.total_keywords ?? 0} total indexed`}
          icon={<Database className="w-5 h-5 text-emerald-400" />}
          badge="SQLite Synced"
        />
        <MetricCard
          title="Trie Nodes in Memory"
          value={summary?.trie_total_nodes ?? 0}
          subtitle={`${summary?.trie_total_words ?? 0} distinct terminal words`}
          icon={<GitFork className="w-5 h-5 text-indigo-400" />}
        />
        <MetricCard
          title="Hash Table Load Factor"
          value={summary ? summary.hash_load_factor.toFixed(3) : '0.000'}
          subtitle={`${summary?.hash_collisions ?? 0} separate chain collisions`}
          icon={<Hash className="w-5 h-5 text-amber-400" />}
          badge="Threshold: 0.75"
        />
      </div>

      {/* Popular and History Section */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Popular Keywords Frequency Chart / List */}
        <div className="bg-slate-900/50 border border-slate-800 rounded-xl p-6">
          <h2 className="text-base font-bold text-white mb-4 flex items-center space-x-2">
            <Activity className="w-4 h-4 text-teal-400" />
            <span>Top Keywords by Frequency</span>
          </h2>

          <div className="space-y-3">
            {popular.length === 0 ? (
              <p className="text-xs text-slate-500">No keywords recorded.</p>
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
                      <span className="text-teal-400 font-mono">{item.frequency} hits</span>
                    </div>
                    <div className="w-full bg-slate-800 rounded-full h-2 overflow-hidden">
                      <div
                        className="bg-gradient-to-r from-teal-500 to-emerald-400 h-2 rounded-full transition-all duration-500"
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
        <div className="bg-slate-900/50 border border-slate-800 rounded-xl p-6">
          <h2 className="text-base font-bold text-white mb-4 flex items-center space-x-2">
            <Search className="w-4 h-4 text-indigo-400" />
            <span>Recent Search Events (Audit Log)</span>
          </h2>

          <div className="space-y-3">
            {history.length === 0 ? (
              <p className="text-xs text-slate-500">No search events recorded yet.</p>
            ) : (
              history.map((item) => (
                <div
                  key={item.id}
                  className="flex items-center justify-between p-3 rounded-lg bg-slate-800/40 border border-slate-700/40 text-xs"
                >
                  <span className="font-medium text-slate-200">{item.keyword}</span>
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

