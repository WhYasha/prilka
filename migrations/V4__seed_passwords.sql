-- V4: Set real PBKDF2-SHA256 password hashes for seed users (password: "testpass")
-- Generated with: PKCS5_PBKDF2_HMAC("testpass", salt, 100000, SHA-256, 32 bytes)
-- Format: pbkdf2$<salt_hex>$<hash_hex>

UPDATE users SET password_hash = 'pbkdf2$d821554c1d77279aeac4a440e55c0c3f$e22e8b99ddc772ddbdc483698928859240389f5f8ff1d1aea9ab9c023375537f'
WHERE username = 'alice' AND password_hash = 'SEED_PLACEHOLDER';

UPDATE users SET password_hash = 'pbkdf2$8f69e0c276b5fc2d6c8f961330747c81$da01bd4643101fed6c8a97444f1ff4f543781c5eb268bb446866a23122cdbafd'
WHERE username = 'bob' AND password_hash = 'SEED_PLACEHOLDER';

UPDATE users SET password_hash = 'pbkdf2$0d9fd821ebf578b7639275bf426f90a1$65bb5c179813f791bb2b1202d9563caeaecb268841ebd8964e8d3c97c52a82fd'
WHERE username = 'carol' AND password_hash = 'SEED_PLACEHOLDER';
