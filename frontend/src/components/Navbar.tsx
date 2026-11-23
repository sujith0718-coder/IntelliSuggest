import React, { useState, useEffect } from 'react';
import { Search, BarChart3, ShieldCheck, Zap, Cpu, Volume2, VolumeX, Command, Sparkles, Globe } from 'lucide-react';
import { sounds } from '../utils/sound';

export type ActiveTab = 'search' | 'forgotten' | 'resources' | 'analytics' | 'admin' | 'performance';

interface NavbarProps {
  activeTab: ActiveTab;
  setActiveTab: (tab: ActiveTab) => void;
  onFocusSearch?: () => void;
}

export const Navbar: React.FC<NavbarProps> = ({ activeTab, setActiveTab, onFocusSearch }) => {
  const [soundOn, setSoundOn] = useState(sounds.isEnabled());

  const toggleSound = () => {
    const newState = sounds.toggle();
    setSoundOn(newState);
  };

  const handleTabChange = (tab: ActiveTab) => {
    sounds.playSelect();
    setActiveTab(tab);
  };

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'k') {
        e.preventDefault();
        setActiveTab('search');
        if (onFocusSearch) onFocusSearch();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [setActiveTab, onFocusSearch]);

  return (
    <header className="border-b border-slate-800/80 bg-slate-950/80 backdrop-blur-md sticky top-0 z-50 transition-colors">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 h-16 flex items-center justify-between">
        {/* Brand identity */}
        <div
          className="flex items-center space-x-3 cursor-pointer group"
          onClick={() => handleTabChange('search')}
          title="IntelliSuggest Home"
        >
          <div className="w-9 h-9 rounded-xl bg-gradient-to-tr from-teal-500 via-emerald-400 to-cyan-400 flex items-center justify-center shadow-lg shadow-teal-500/20 group-hover:scale-105 transition-transform">
            <Cpu className="w-5 h-5 text-slate-950 font-bold" />
          </div>
          <div>
            <div className="flex items-center space-x-2">
              <span className="font-extrabold text-base text-white tracking-tight">IntelliSuggest</span>
              <span className="text-[10px] font-mono px-2 py-0.5 rounded-full bg-teal-500/10 text-teal-400 border border-teal-500/20 font-semibold">
                C11 Core
              </span>
            </div>
            <p className="text-[11px] text-slate-400 font-medium">Team 12 · Academic DSA Engine</p>
          </div>
        </div>

        {/* Navigation Tabs */}
        <nav className="flex items-center space-x-1 sm:space-x-1.5 bg-slate-900/60 p-1 rounded-xl border border-slate-800/60 overflow-x-auto max-w-[65%] sm:max-w-none">
          {/* 1. Intelligent Search */}
          <button
            onClick={() => handleTabChange('search')}
            className={`flex items-center space-x-1.5 px-2.5 sm:px-3 py-1.5 rounded-lg text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'search'
                ? 'bg-teal-500 text-slate-950 shadow-md shadow-teal-500/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <Search className="w-3.5 h-3.5" />
            <span className="hidden md:inline">Intelligent Search</span>
            <span className="md:hidden">Search</span>
          </button>

          {/* 2. Forgotten-Name Search */}
          <button
            onClick={() => handleTabChange('forgotten')}
            className={`flex items-center space-x-1.5 px-2.5 sm:px-3 py-1.5 rounded-lg text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'forgotten'
                ? 'bg-teal-500 text-slate-950 shadow-md shadow-teal-500/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <Sparkles className="w-3.5 h-3.5" />
            <span className="hidden md:inline">Forgotten-Name</span>
            <span className="md:hidden">Forgotten</span>
          </button>

          {/* 3. Resource Explorer (Google PSE) */}
          <button
            onClick={() => handleTabChange('resources')}
            className={`flex items-center space-x-1.5 px-2.5 sm:px-3 py-1.5 rounded-lg text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'resources'
                ? 'bg-blue-600 text-white shadow-md shadow-blue-600/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <Globe className="w-3.5 h-3.5" />
            <span className="hidden md:inline">Resource Explorer</span>
            <span className="md:hidden">Resources</span>
          </button>

          {/* 4. Analytics */}
          <button
            onClick={() => handleTabChange('analytics')}
            className={`flex items-center space-x-1.5 px-2.5 sm:px-3 py-1.5 rounded-lg text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'analytics'
                ? 'bg-teal-500 text-slate-950 shadow-md shadow-teal-500/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <BarChart3 className="w-3.5 h-3.5" />
            <span>Analytics</span>
          </button>

          {/* 5. Admin */}
          <button
            onClick={() => handleTabChange('admin')}
            className={`flex items-center space-x-1.5 px-2.5 sm:px-3 py-1.5 rounded-lg text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'admin'
                ? 'bg-teal-500 text-slate-950 shadow-md shadow-teal-500/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <ShieldCheck className="w-3.5 h-3.5" />
            <span>Admin</span>
          </button>

          {/* 6. Performance */}
          <button
            onClick={() => handleTabChange('performance')}
            className={`flex items-center space-x-1.5 px-2.5 sm:px-3 py-1.5 rounded-lg text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'performance'
                ? 'bg-teal-500 text-slate-950 shadow-md shadow-teal-500/20'
                : 'text-slate-400 hover:text-white hover:bg-slate-800/60'
            }`}
          >
            <Zap className="w-3.5 h-3.5" />
            <span className="hidden sm:inline">Performance</span>
            <span className="sm:hidden">Perf</span>
          </button>
        </nav>

        {/* Global Controls & Keyboard Indicator */}
        <div className="flex items-center space-x-2">
          <button
            onClick={() => {
              setActiveTab('search');
              if (onFocusSearch) onFocusSearch();
            }}
            className="hidden lg:flex items-center space-x-1 px-2.5 py-1 rounded-lg bg-slate-900 border border-slate-800 text-[11px] font-mono text-slate-400 hover:border-slate-700 hover:text-slate-300 transition-colors"
            title="Focus search input"
          >
            <Command className="w-3 h-3 text-slate-500" />
            <span>K</span>
          </button>

          <button
            onClick={toggleSound}
            className={`p-2 rounded-lg border transition-colors ${
              soundOn
                ? 'bg-slate-900 border-slate-700 text-teal-400 hover:bg-slate-800'
                : 'bg-slate-900/50 border-slate-800 text-slate-500 hover:text-slate-400'
            }`}
            title={soundOn ? 'Sound feedback active (Click to mute)' : 'Sound feedback muted (Click to unmute)'}
          >
            {soundOn ? <Volume2 className="w-4 h-4" /> : <VolumeX className="w-4 h-4" />}
          </button>
        </div>
      </div>
    </header>
  );
};
