{{- with secret "secret/data/messenger/db" -}}
DB_HOST={{ .Data.data.host }}
DB_PORT={{ .Data.data.port }}
DB_NAME={{ .Data.data.name }}
DB_USER={{ .Data.data.user }}
DB_PASS={{ .Data.data.password }}
{{ end -}}

{{- with secret "secret/data/messenger/redis" -}}
REDIS_HOST={{ .Data.data.host }}
REDIS_PORT={{ .Data.data.port }}
REDIS_PASS={{ .Data.data.password }}
{{ end -}}

{{- with secret "secret/data/messenger/minio" -}}
MINIO_ENDPOINT={{ .Data.data.endpoint }}
MINIO_ACCESS_KEY={{ .Data.data.access_key }}
MINIO_SECRET_KEY={{ .Data.data.secret_key }}
MINIO_PUBLIC_URL={{ .Data.data.public_url }}
MINIO_AVATARS_BUCKET={{ .Data.data.avatars_bucket }}
MINIO_UPLOADS_BUCKET={{ .Data.data.uploads_bucket }}
MINIO_STICKERS_BUCKET={{ .Data.data.stickers_bucket }}
MINIO_PRESIGN_TTL={{ .Data.data.presign_ttl }}
{{ end -}}

{{- with secret "secret/data/messenger/jwt" -}}
JWT_SECRET={{ .Data.data.secret }}
JWT_ACCESS_TTL={{ .Data.data.access_ttl }}
JWT_REFRESH_TTL={{ .Data.data.refresh_ttl }}
{{ end -}}

{{- with secret "secret/data/messenger/server" -}}
API_PORT={{ .Data.data.port }}
API_THREADS={{ .Data.data.threads }}
MAX_FILE_SIZE_MB={{ .Data.data.max_file_size_mb }}
{{ end -}}
