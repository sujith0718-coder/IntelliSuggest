# IntelliSuggest Deployment Guide

This document describes how to deploy the IntelliSuggest full-stack application to **Render** (Backend Docker Web Service) and **Vercel** (Frontend optional decoupled setup), adhering strictly to non-ephemeral SQLite persistence and high-performance in-memory C DSA execution.

---

## 1. Deploying to Render (Docker Web Service)

Render builds and runs the unified container directly from the root `Dockerfile`.

### Service Details:
- **Service Type**: Web Service
- **Repository**: `sujith0718-coder/IntelliSuggest`
- **Branch**: `main`
- **Region**: Singapore (Southeast Asia)
- **Environment**: Docker
- **Root Directory**: `.` (leave empty or set to repository root)
- **Dockerfile Path**: `Dockerfile`

### Render Environment Variables:
| Variable Name | Recommended Value | Description |
| :--- | :--- | :--- |
| `PORT` | `10000` | Render default web service port (CivetWeb binds to `0.0.0.0:$PORT`) |
| `DATABASE_PATH` | `/var/data/intellisuggest.db` | Path to persistent SQLite database on Render disk |
| `SAMPLE_DATA_PATH` | `/app/database/sample_data.csv` | Dataset used for initial database seeding |
| `CORS_ORIGIN` | `*` *(or your Vercel URL)* | Allowed CORS origin (default reflects client or `*`) |

### Health Check Configuration:
- **Health Check Path**: `/api/health` (or `/health`)
- Response: `{"status": "ok", "service": "IntelliSuggest C DSA Core"}` with HTTP 200 OK.

### Persistent Disk Configuration (Render Disks):
Render containers have an ephemeral root filesystem. To preserve search frequencies, keyword additions, and history across restarts and redeploys:
1. In the Web Service settings on Render, navigate to **Disks**.
2. Click **Add Disk**:
   - **Name**: `intellisuggest-db-disk`
   - **Mount Path**: `/var/data`
   - **Size**: `1 GB` (sufficient for hundreds of thousands of keywords)

---

## 2. Deploying Frontend to Vercel (Optional Decoupled Setup)

The Docker container on Render already serves the production React frontend bundle at `/` via CivetWeb. However, if you also wish to deploy the React frontend independently on Vercel:

1. Import the repository into **Vercel**.
2. Set **Root Directory**: `frontend`.
3. Framework Preset: **Vite**.
4. Build Command: `npm run build`.
5. Output Directory: `dist`.
6. Add the Environment Variable in Vercel:
   - `VITE_API_BASE_URL`: `https://your-intellisuggest-service.onrender.com`
7. Deploy. CivetWeb's dynamic CORS handler will automatically allow requests from your `*.vercel.app` domain.

---

## 3. Local Docker Testing

To test the containerized service locally with Docker Compose:

```bash
docker-compose up --build -d
```
Access points:
- Web Application: `http://localhost:10000/`
- Health Check: `http://localhost:10000/api/health`
- Autocomplete API: `http://localhost:10000/api/suggestions?prefix=dijk&mode=cached`
