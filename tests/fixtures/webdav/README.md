# Isolated WebDAV TLS fixture

The certificate and publicly committed test key are generated solely for loopback tests of remote.example.test. They are not production credentials. RemoteBackupTests trusts this CA only in its own Qt process, without modifying any Windows certificate store. The test bridge maps the HTTPS connection to an in-memory DAV server. No public DNS/server/account is used.

Generated using OpenSSL req -x509, RSA 2048, subjectAltName DNS:remote.example.test, ten-year test validity. Production code never loads these files or bypasses certificate verification.
