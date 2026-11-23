# IntelliSuggest Deployment Guide

This document describes how to deploy the IntelliSuggest full-stack application to **Render** (Backend & Static bundle) and **Vercel** (Frontend alternate), adhering strictly to non-ephemeral SQLite persistence rules.

---

## 1. Deploying to Render (Full-Stack Container)

Render supports Docker-based deployments with persistent disks.

### Configuration Steps:
1. **Create a new Web Service** on Render connected to the GitHub repository.
2. Select **Docker** as the Environment.
3. In **Advanced Settings**, configure:
   - **Environment Variable**: `PORT = 10000` (Render dynamically injects this).
   - **Disk**: Add a Persistent Disk mounted at `/app/data` with 1 GB capacity.
4. Render will execute the multi-stage `Dockerfile`:
   - Stage 1 compiles the React + Vite frontend to static HTML/JS/CSS assets.
   - Stage 2 compiles the C11 DSA engine with CivetWeb and SQLite.
   - Stage 3 runs the lightweight container serving both the API at `/api/*` and the React UI at `/`.

---

## 2. Deploying Frontend to Vercel (Optional Decoupled Setup)

If you prefer deploying the React frontend independently on Vercel:

1. Import the `frontend/` folder into Vercel.
2. Framework Preset: **Vite**.
3. Build Command: `npm run build`
4. Output Directory: `dist`
5. Configure `VITE_API_BASE_URL` pointing to the Render backend URL (e.g. `https://intellisuggest.onrender.com`).
6. CivetWeb CORS headers (`Access-Control-Allow-Origin: *`) are pre-configured to allow cross-origin requests from Vercel.

---

## 3. Local Docker Execution

To test the containerized system locally:

```bash
# Build and run container
docker-compose up --build -d

# Verify server logs
docker logs -f intellisuggest-app

# Open in browser:
# http://localhost:8080
```
