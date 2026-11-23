import React, { useState } from 'react';
import { Navbar } from './components/Navbar';
import { SearchPage } from './pages/Search';
import { AnalyticsPage } from './pages/Analytics';
import { AdminPage } from './pages/Admin';
import { PerformancePage } from './pages/Performance';

export const App: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'search' | 'analytics' | 'admin' | 'performance'>('search');

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col font-sans selection:bg-teal-500 selection:text-slate-950">
      <Navbar activeTab={activeTab} setActiveTab={setActiveTab} />
      <main className="flex-1">
        {activeTab === 'search' && <SearchPage />}
        {activeTab === 'analytics' && <AnalyticsPage />}
        {activeTab === 'admin' && <AdminPage />}
        {activeTab === 'performance' && <PerformancePage />}
      </main>
      <footer className="border-t border-slate-800/60 py-6 text-center text-xs text-slate-500">
        <p>IntelliSuggest · Academic Data Structures and Algorithms Project · Team 12</p>
        <p className="mt-1 font-mono text-[11px] text-slate-600">
          Engine: C11 (Trie + Hash Table + Min Heap) · Persistence: SQLite · Server: CivetWeb · Frontend: React + TypeScript
        </p>
      </footer>
    </div>
  );
};

export default App;

