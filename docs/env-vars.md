# Environment Variables Reference

Copy `.env.example` to `.env` and adjust values before running `make up`.

## PostgreSQL

| Variable | Default | Description |
|----------|---------|-------------|
| `POSTGRES_DB` | `messenger` | Database name |
| `POSTGRES_USER` | `messenger` | Database user |
| `POSTGRES_PASSWORD` | *(required)* | Database password — change before deploy |

## Redis

| Variable | Default | Description |
|----------|---------|-------------|
| `REDIS_PASSWORD` | *(required)* | Redis AUTH password |

## MinIO

| Variable | Default | Description |
|----------|---------|-------------|
| `MINIO_ROOT_USER` | `minioadmin` | MinIO root access key |
| `MINIO_ROOT_PASSWORD` | *(required)* | MinIO root secret key |
| `MINIO_BUCKET` | `messenger-files` | Default bucket name |
| `MINIO_ENDPOINT` | `minio:9000` | Internal MinIO address (used by api_cpp) |

## JWT

| Variable | Default | Description |
|----------|---------|-------------|
| `JWT_SECRET` | *(required)* | Signing secret — min 32 chars, random; use `openssl rand -hex 32` |
| `JWT_ACCESS_TTL` | `3600` | Access token lifetime (seconds) |
| `JWT_REFRESH_TTL` | `604800` | Refresh token lifetime (seconds, default 7 days) |

## File uploads

| Variable | Default | Description |
|----------|---------|-------------|
| `MAX_FILE_SIZE_MB` | `50` | Maximum upload size in megabytes |

## API server

| Variable | Default | Description |
|----------|---------|-------------|
| `API_PORT` | `8080` | Port for the C++ API server |
| `API_THREADS` | `0` | IO threads (0 = auto = number of CPU cores) |

## Grafana

| Variable | Default | Description |
|----------|---------|-------------|
| `GRAFANA_USER` | `admin` | Grafana admin username |
| `GRAFANA_PASSWORD` | `admin` | Grafana admin password — change in production |

## Production Checklist
- [ ] `JWT_SECRET` — generated with `openssl rand -hex 32`
- [ ] All passwords changed from defaults
- [ ] `POSTGRES_PASSWORD` stored in a secrets manager
- [ ] `GRAFANA_PASSWORD` changed and 2FA enabled
- [ ] MinIO TLS enabled (`minioUseSSL=true`)
- [ ] Rate limiting configured (nginx / Redis)
