# IntelliSuggest Deployment Guide

This document describes how to deploy the IntelliSuggest full-stack application to **Render** and **Vercel**, featuring the **Dual-Driver Database Architecture** (Local Embedded SQLite for development and Remote SQLiteCloud for production).

---

## 1. Dual-Driver Database Architecture Overview

IntelliSuggest features two isolated database drivers selectable via environment configuration:

1. **Local Development Driver (`DATABASE_DRIVER=sqlite`)**:
   - Uses embedded `sqlite3.c` with a local database file (`intellisuggest.db`).
   - Requires zero network access.
   - All 132 C unit tests and local development run completely offline.

2. **Production Cloud Driver (`DATABASE_DRIVER=sqlitecloud`)**:
   - Connects to remote **SQLiteCloud** via HTTPS gateway (`https://<project>.gateway.sqlite.cloud/v2/weblite/sql`).
   - Targets the dedicated cloud database: **`intellisuggest.sqlite`** (separate from any other databases such as `chinook.sqlite`).
   - Provides remote persistent storage across container restarts and redeployments.
   - **Render does not require a paid persistent disk** when running on SQLiteCloud!

---

## 2. Deploying to Render (Docker Web Service)

Render builds and runs the unified container directly from the root `Dockerfile`.

### Render Web Service Settings:
- **Service Type**: Web Service
- **Repository**: `sujith0718-coder/IntelliSuggest`
- **Branch**: `main`
- **Region**: Singapore (Southeast Asia)
- **Environment**: Docker
- **Root Directory**: `.` (leave empty or repository root)
- **Dockerfile Path**: `Dockerfile`

### Required Environment Variables on Render:

| Variable Name | Recommended Value | Description |
| :--- | :--- | :--- |
| `PORT` | `10000` | Render listening port (CivetWeb binds to `0.0.0.0:10000`) |
| `DATABASE_DRIVER` | `sqlitecloud` | Activates production SQLiteCloud remote driver |
| `SQLITECLOUD_URL` | `sqlitecloud://<host>:8860/intellisuggest.sqlite?apikey=<api_key>` | Complete connection URI to SQLiteCloud |
| `CORS_ORIGIN` | `*` *(or your Vercel URL)* | Allows cross-origin requests from your frontend |

### Health Check Configuration:
- **Health Check Path**: `/api/health` (or `/health`)
- Health response includes active driver status:
  ```json
  {
    "status": "ok",
    "service": "IntelliSuggest C DSA Core",
    "database_driver": "SQLiteCloud (Remote)"
  }
  ```

---

## 3. Deploying Frontend to Vercel (Optional Decoupled Setup)

The unified Docker container on Render already serves the production React frontend bundle at `/` via CivetWeb. However, if you also wish to deploy the React frontend independently on Vercel:

1. Import the repository into **Vercel**.
2. Set **Root Directory**: `frontend`.
3. Framework Preset: **Vite**.
4. Build Command: `npm run build`.
5. Output Directory: `dist`.
6. Add Environment Variable in Vercel:
   - `VITE_API_BASE_URL`: `https://<your-service-name>.onrender.com`
7. Deploy. CivetWeb's dynamic CORS handler will automatically allow requests from your `*.vercel.app` domain.

---

## 4. Local Execution & Testing

### Running locally with SQLite (default):
```bash
# Clean local build
cmake --build build --config Release

# Run server with local embedded SQLite
./build/backend/bin/intellisuggest
```

### Running locally with SQLiteCloud:
```bash
# In PowerShell:
$env:DATABASE_DRIVER = "sqlitecloud"
$env:SQLITECLOUD_URL = "sqlitecloud://<host>:8860/intellisuggest.sqlite?apikey=<api_key>"
./build/backend/bin/intellisuggest.exe
```

### Running Test Suites:
```bash
# Run all 132 local unit tests (100% offline)
./build/backend/bin/test_trie
./build/backend/bin/test_hashmap
./build/backend/bin/test_heap
./build/backend/bin/test_ranking
./build/backend/bin/test_database
./build/backend/bin/test_forgotten_search

# Run live SQLiteCloud integration test suite
./build/backend/bin/test_sqlitecloud
```
