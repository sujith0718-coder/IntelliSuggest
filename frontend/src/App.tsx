import React, { useState, useRef } from 'react';
import { Navbar, ActiveTab } from './components/Navbar';
import { SearchPage } from './pages/Search';
import { ResourceExplorerPage } from './pages/ResourceExplorer';
import { AnalyticsPage } from './pages/Analytics';
import { AdminPage } from './pages/Admin';
import { PerformancePage } from './pages/Performance';

export const App: React.FC = () => {
  const [activeTab, setActiveTab] = useState<ActiveTab>('search');
  const searchInputRef = useRef<HTMLInputElement>(null);

  const handleFocusSearch = () => {
    setActiveTab('search');
    setTimeout(() => {
      searchInputRef.current?.focus();
    }, 50);
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col font-sans selection:bg-teal-500 selection:text-slate-950 antialiased">
      <Navbar
        activeTab={activeTab}
        setActiveTab={setActiveTab}
        onFocusSearch={handleFocusSearch}
      />
      <main className="flex-1">
        {activeTab === 'search' && (
          <SearchPage searchInputRef={searchInputRef} initialMode="prefix" />
        )}
        {activeTab === 'forgotten' && (
          <SearchPage searchInputRef={searchInputRef} initialMode="forgotten" />
        )}
        {activeTab === 'resources' && <ResourceExplorerPage />}
        {activeTab === 'analytics' && <AnalyticsPage />}
        {activeTab === 'admin' && <AdminPage />}
        {activeTab === 'performance' && <PerformancePage />}
      </main>
      <footer className="border-t border-slate-800/60 py-6 text-center text-xs text-slate-500 bg-slate-950/80">
        <p className="font-medium text-slate-400">IntelliSuggest · Academic Data Structures and Algorithms Project · Team 12</p>
        <p className="mt-1 font-mono text-[11px] text-slate-500">
          In-Memory Core: C11 (Trie + Hash Table + Min Heap) · Persistence: SQLiteCloud · External Discovery: Google PSE · Frontend: React 18 + TS
        </p>
      </footer>
    </div>
  );
};

export default App;
