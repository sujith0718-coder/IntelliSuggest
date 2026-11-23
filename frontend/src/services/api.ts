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
  is_demo?: boolean;
}

export interface PopularItem {
  id: number;
  keyword: string;
  frequency: number;
  last_searched_at?: string;
}

export interface AnalyticsSummary {
  total_keywords: number;
  active_keywords: number;
  expired_keywords?: number;
  total_searches: number;
  demo_searches?: number;
  trie_total_words: number;
  trie_total_nodes: number;
  hash_load_factor: number;
  hash_collisions: number;
  pending_writes?: number;
  persisted_writes?: number;
  failed_writes?: number;
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

export interface HealthStatus {
  status: string;
  uptime_seconds?: number;
  total_keywords?: number;
  total_searches?: number;
  persistence_queue?: {
    pending_writes: number;
    persisted_writes: number;
    failed_writes: number;
    status: string;
  };
}

export interface AdminConceptRecord {
  id: number;
  concept_name: string;
  description: string;
  keywords: string;
}

const API_BASE = (import.meta.env.VITE_API_BASE_URL ? import.meta.env.VITE_API_BASE_URL.replace(/\/+$/, '') : '') + '/api';

/* Admin Session Token Management (stored in sessionStorage for session scope) */
export const DEFAULT_ADMIN_TOKEN = 'isug_admin_19344350b5f402c3b81d441639ce97104988cada0dbac27b';

let adminToken: string | null = typeof window !== 'undefined'
  ? (sessionStorage.getItem('intellisuggest_admin_token') || DEFAULT_ADMIN_TOKEN)
  : DEFAULT_ADMIN_TOKEN;

export const setAdminToken = (token: string | null) => {
  adminToken = token;
  if (typeof window !== 'undefined') {
    if (token) {
      sessionStorage.setItem('intellisuggest_admin_token', token);
    } else {
      sessionStorage.removeItem('intellisuggest_admin_token');
    }
  }
};

export const getAdminToken = (): string | null => adminToken;

const getAuthHeaders = (): Record<string, string> => {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (adminToken) {
    headers['Authorization'] = `Bearer ${adminToken}`;
  }
  return headers;
};

export const api = {
  /* Public Search & Suggestions */
  async getSuggestions(prefix: string, mode: 'cached' | 'heap' = 'cached'): Promise<SuggestionResponse> {
    const res = await fetch(`${API_BASE}/suggestions?prefix=${encodeURIComponent(prefix)}&mode=${mode}`);
    if (!res.ok) throw new Error('Failed to fetch suggestions');
    return res.json();
  },

  async submitSearch(keyword: string): Promise<{ success: boolean; keyword: string; new_frequency: number; event_id?: string; persistence_status?: string }> {
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

  /* Analytics & Monitoring */
  async getHealth(): Promise<HealthStatus> {
    const res = await fetch(`${API_BASE}/health`);
    if (!res.ok) throw new Error('Failed to fetch health status');
    return res.json();
  },

  async getAnalytics(): Promise<AnalyticsSummary> {
    const res = await fetch(`${API_BASE}/analytics`);
    if (!res.ok) throw new Error('Failed to fetch analytics');
    return res.json();
  },

  async getTrending(): Promise<{ trending: PopularItem[] }> {
    const res = await fetch(`${API_BASE}/analytics/trending`);
    if (!res.ok) throw new Error('Failed to fetch trending keywords');
    return res.json();
  },

  async getExpired(): Promise<{ expired: KeywordRecord[] }> {
    const res = await fetch(`${API_BASE}/analytics/expired`);
    if (!res.ok) throw new Error('Failed to fetch expired keywords');
    return res.json();
  },

  async getPerformance(): Promise<PerformanceResponse> {
    const res = await fetch(`${API_BASE}/performance`);
    if (!res.ok) throw new Error('Failed to fetch performance metrics');
    return res.json();
  },

  /* Keyword Administration */
  async getKeywords(): Promise<{ keywords: KeywordRecord[] }> {
    const res = await fetch(`${API_BASE}/keywords`);
    if (!res.ok) throw new Error('Failed to fetch keywords');
    return res.json();
  },

  async addKeyword(keyword: string, frequency: number): Promise<{ success: boolean; id: number }> {
    const res = await fetch(`${API_BASE}/keywords`, {
      method: 'POST',
      headers: getAuthHeaders(),
      body: JSON.stringify({ keyword, frequency }),
    });
    if (res.status === 401) throw new Error('Unauthorized: Valid ADMIN_API_KEY required');
    if (!res.ok) throw new Error('Failed to add keyword');
    return res.json();
  },

  async updateKeyword(id: number, keyword: string, frequency: number, is_active: boolean): Promise<{ success: boolean }> {
    const res = await fetch(`${API_BASE}/keywords`, {
      method: 'PUT',
      headers: getAuthHeaders(),
      body: JSON.stringify({ id, keyword, frequency, is_active }),
    });
    if (res.status === 401) throw new Error('Unauthorized: Valid ADMIN_API_KEY required');
    if (!res.ok) throw new Error('Failed to update keyword');
    return res.json();
  },

  async deleteKeyword(id: number): Promise<{ success: boolean; deleted_keyword: string }> {
    const res = await fetch(`${API_BASE}/keywords?id=${id}`, {
      method: 'DELETE',
      headers: getAuthHeaders(),
    });
    if (res.status === 401) throw new Error('Unauthorized: Valid ADMIN_API_KEY required');
    if (!res.ok) throw new Error('Failed to delete keyword');
    return res.json();
  },

  async cleanupExpired(): Promise<{ success: boolean; cleaned_count: number }> {
    const res = await fetch(`${API_BASE}/keywords/cleanup`, {
      method: 'POST',
      headers: getAuthHeaders(),
    });
    if (res.status === 401) throw new Error('Unauthorized: Valid ADMIN_API_KEY required');
    if (!res.ok) throw new Error('Failed to run keyword cleanup');
    return res.json();
  },

  /* Concept Administration (Phase 4) */
  async getConcepts(): Promise<{ concepts: AdminConceptRecord[] }> {
    const res = await fetch(`${API_BASE}/concepts`);
    if (!res.ok) throw new Error('Failed to fetch concepts');
    return res.json();
  },

  async addConcept(concept_name: string, description: string, keywords: string): Promise<{ success: boolean; id: number }> {
    const res = await fetch(`${API_BASE}/concepts`, {
      method: 'POST',
      headers: getAuthHeaders(),
      body: JSON.stringify({ concept_name, description, keywords }),
    });
    if (res.status === 401) throw new Error('Unauthorized: Valid ADMIN_API_KEY required');
    if (!res.ok) throw new Error('Failed to create concept');
    return res.json();
  },

  async updateConcept(id: number, concept_name: string, description: string, keywords: string): Promise<{ success: boolean }> {
    const res = await fetch(`${API_BASE}/concepts`, {
      method: 'PUT',
      headers: getAuthHeaders(),
      body: JSON.stringify({ id, concept_name, description, keywords }),
    });
    if (res.status === 401) throw new Error('Unauthorized: Valid ADMIN_API_KEY required');
    if (!res.ok) throw new Error('Failed to update concept');
    return res.json();
  },

  async deleteConcept(id: number): Promise<{ success: boolean }> {
    const res = await fetch(`${API_BASE}/concepts?id=${id}`, {
      method: 'DELETE',
      headers: getAuthHeaders(),
    });
    if (res.status === 401) throw new Error('Unauthorized: Valid ADMIN_API_KEY required');
    if (!res.ok) throw new Error('Failed to delete concept');
    return res.json();
  },
};
