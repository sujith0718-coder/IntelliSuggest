# ==============================================================================
# Multi-stage Dockerfile for IntelliSuggest (C Backend + React Frontend)
# Optimized for Render Web Services & Containerized Deployments
# ==============================================================================

# STAGE 1: Build React Frontend
FROM node:20-alpine AS frontend-builder
WORKDIR /app/frontend

COPY frontend/package*.json ./
RUN npm ci

COPY frontend/ ./
RUN npm run build

# STAGE 2: Build C Backend
FROM alpine:3.19 AS backend-builder
RUN apk add --no-cache build-base cmake

WORKDIR /app
COPY backend/ ./backend/
COPY database/ ./database/
COPY CMakeLists.txt ./

# Configure and compile release binary
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --target intellisuggest

# STAGE 3: Final Minimal Runtime Image
FROM alpine:3.19
RUN apk add --no-cache libgcc libstdc++ ca-certificates

WORKDIR /app

# Copy compiled C binary
COPY --from=backend-builder /app/build/backend/bin/intellisuggest /app/intellisuggest

# Copy prebuilt React frontend assets (served by embedded CivetWeb)
COPY --from=frontend-builder /app/frontend/dist /app/frontend/dist

# Copy seed dataset and schema
COPY database/sample_data.csv /app/database/sample_data.csv
COPY database/schema.sql /app/database/schema.sql

# Create persistent storage mount points (Render disk default is /var/data)
RUN mkdir -p /var/data /app/data && chmod 777 /var/data /app/data
VOLUME ["/var/data", "/app/data"]

# Default Render environment configuration
ENV PORT=10000
ENV DATABASE_PATH=/var/data/intellisuggest.db
ENV SAMPLE_DATA_PATH=/app/database/sample_data.csv

EXPOSE 10000

CMD ["/app/intellisuggest"]
