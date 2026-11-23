import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  stages: [
    { duration: '5s', target: 20 },   // Ramp up to 20 virtual users
    { duration: '15s', target: 50 },  // Sustained load with 50 virtual users
    { duration: '5s', target: 0 },    // Ramp down
  ],
  thresholds: {
    http_req_duration: ['p(95)<10', 'p(99)<25'], // 95% of requests under 10ms
    http_req_failed: ['rate<0.01'],              // Error rate below 1%
  },
};

const BASE_URL = __ENV.BASE_URL || 'http://localhost:8080';

const TEST_PREFIXES = ['p', 'pr', 'pro', 'd', 'da', 'data', 'm', 'me', 's', 'se'];

export default function () {
  const prefix = TEST_PREFIXES[Math.floor(Math.random() * TEST_PREFIXES.length)];

  // 1. Test Autocomplete Suggestions
  const res = http.get(`${BASE_URL}/api/suggestions?prefix=${prefix}&mode=cached`);
  check(res, {
    'status is 200': (r) => r.status === 200,
    'has suggestions': (r) => {
      try {
        const body = JSON.parse(r.body);
        return Array.isArray(body.suggestions);
      } catch (e) {
        return false;
      }
    },
  });

  // 2. Occasionally submit a search (1 in 5 iterations)
  if (Math.random() < 0.2) {
    const payload = JSON.stringify({ keyword: 'programming' });
    const postRes = http.post(`${BASE_URL}/api/search`, payload, {
      headers: { 'Content-Type': 'application/json' },
    });
    check(postRes, {
      'search recorded': (r) => r.status === 200,
    });
  }

  sleep(0.05);
}

