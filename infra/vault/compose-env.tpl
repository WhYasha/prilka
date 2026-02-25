# ============================================================
# Vault Agent template → Docker Compose .env
# Rendered by: vault agent -config=infra/vault/agent-init.hcl
# This produces the .env file that docker-compose.yml reads.
# ============================================================

# ----- PostgreSQL -----
{{- with secret "secret/data/messenger/db" }}
POSTGRES_DB={{ .Data.data.name }}
POSTGRES_USER={{ .Data.data.user }}
POSTGRES_PASSWORD={{ .Data.data.password }}
{{- end }}

# ----- Redis -----
{{- with secret "secret/data/messenger/redis" }}
REDIS_PASSWORD={{ .Data.data.password }}
{{- end }}

# ----- MinIO -----
{{- with secret "secret/data/messenger/minio" }}
MINIO_ROOT_USER={{ .Data.data.access_key }}
MINIO_ROOT_PASSWORD={{ .Data.data.secret_key }}
MINIO_ENDPOINT={{ .Data.data.endpoint }}
MINIO_AVATARS_BUCKET={{ .Data.data.avatars_bucket }}
MINIO_UPLOADS_BUCKET={{ .Data.data.uploads_bucket }}
MINIO_STICKERS_BUCKET={{ .Data.data.stickers_bucket }}
MINIO_TESTS_BUCKET={{ .Data.data.tests_bucket }}
MINIO_PRESIGN_TTL={{ .Data.data.presign_ttl }}
MINIO_PUBLIC_URL={{ .Data.data.public_url }}
{{- end }}

# ----- JWT -----
{{- with secret "secret/data/messenger/jwt" }}
JWT_SECRET={{ .Data.data.secret }}
JWT_ACCESS_TTL={{ .Data.data.access_ttl }}
JWT_REFRESH_TTL={{ .Data.data.refresh_ttl }}
{{- end }}

# ----- API server -----
{{- with secret "secret/data/messenger/server" }}
API_PORT={{ .Data.data.port }}
API_THREADS={{ .Data.data.threads }}
MAX_FILE_SIZE_MB={{ .Data.data.max_file_size_mb }}
{{- end }}

# ----- Grafana -----
{{- with secret "secret/data/messenger/grafana" }}
GRAFANA_USER={{ .Data.data.user }}
GRAFANA_PASSWORD={{ .Data.data.password }}
{{- end }}
