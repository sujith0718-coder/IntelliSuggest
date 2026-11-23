export interface Suggestion {
  keyword: string;
  frequency: number;
}

export interface SuggestionResponse {
  prefix: string;
  mode: 'cached' | 'heap';
  count: number;
  latency_us: number;
  suggestions: Suggestion[];
}

export interface ConceptResult {
  concept_name: string;
  description: string;
  relevance_score: number;
  related_keywords: string[];
}

export interface ForgottenSearchResponse {
  query: string;
  count: number;
  results: ConceptResult[];
}

export interface HistoryItem {
  id: number;
  keyword: string;
  searched_at: string;
}

export interface PopularItem {
  id: number;
  keyword: string;
  frequency: number;
}

export interface AnalyticsSummary {
  total_keywords: number;
  active_keywords: number;
  total_searches: number;
  trie_total_words: number;
  trie_total_nodes: number;
  hash_load_factor: number;
  hash_collisions: number;
}

export interface PerformanceResponse {
  comparison: {
    approach_a: {
      name: string;
      complexity: string;
      sample_latency_us: number;
    };
    approach_b: {
      name: string;
      complexity: string;
      sample_latency_us: number;
    };
  };
}

export interface KeywordRecord {
  id: number;
  keyword: string;
  frequency: number;
  created_at: string;
  last_searched_at: string;
  is_active: boolean;
}

const API_BASE = (import.meta.env.VITE_API_BASE_URL ? import.meta.env.VITE_API_BASE_URL.replace(/\/+$/, '') : '') + '/api';

export const api = {
  async getSuggestions(prefix: string, mode: 'cached' | 'heap' = 'cached'): Promise<SuggestionResponse> {
    const res = await fetch(`${API_BASE}/suggestions?prefix=${encodeURIComponent(prefix)}&mode=${mode}`);
    if (!res.ok) throw new Error('Failed to fetch suggestions');
    return res.json();
  },

  async submitSearch(keyword: string): Promise<{ success: boolean; keyword: string; new_frequency: number }> {
    const res = await fetch(`${API_BASE}/search`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ keyword }),
    });
    if (!res.ok) throw new Error('Failed to submit search');
    return res.json();
  },

  async getPopular(): Promise<{ popular: PopularItem[] }> {
    const res = await fetch(`${API_BASE}/popular`);
    if (!res.ok) throw new Error('Failed to fetch popular keywords');
    return res.json();
  },

  async getHistory(): Promise<{ history: HistoryItem[] }> {
    const res = await fetch(`${API_BASE}/history`);
    if (!res.ok) throw new Error('Failed to fetch search history');
    return res.json();
  },

  async forgottenSearch(description: string): Promise<ForgottenSearchResponse> {
    const res = await fetch(`${API_BASE}/forgotten-search`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ description }),
    });
    if (!res.ok) throw new Error('Failed to perform concept search');
    return res.json();
  },

  async getAnalytics(): Promise<AnalyticsSummary> {
    const res = await fetch(`${API_BASE}/analytics`);
    if (!res.ok) throw new Error('Failed to fetch analytics');
    return res.json();
  },

  async getPerformance(): Promise<PerformanceResponse> {
    const res = await fetch(`${API_BASE}/performance`);
    if (!res.ok) throw new Error('Failed to fetch performance metrics');
    return res.json();
  },

  async getKeywords(): Promise<{ keywords: KeywordRecord[] }> {
    const res = await fetch(`${API_BASE}/keywords`);
    if (!res.ok) throw new Error('Failed to fetch keywords');
    return res.json();
  },

  async addKeyword(keyword: string, frequency: number): Promise<{ success: boolean; id: number }> {
    const res = await fetch(`${API_BASE}/keywords`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ keyword, frequency }),
    });
    if (!res.ok) throw new Error('Failed to add keyword');
    return res.json();
  },

  async cleanupExpired(): Promise<{ success: boolean; cleaned_count: number }> {
    const res = await fetch(`${API_BASE}/keywords/cleanup`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
    });
    if (!res.ok) throw new Error('Failed to run keyword cleanup');
    return res.json();
  },
};

