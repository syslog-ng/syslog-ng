# Repeatable Docker Image Publishing

This document explains how to use the enhanced `publish-docker-image.yml` workflow for reproducible Docker image builds.

## Quick Reference

**Most Common Use Cases:**

| Scenario | pkg-type | pkg-version | apt-pkg-version | base-image | Use Case |
|----------|----------|-------------------|---------------------|------------|----------|
| **Publish latest stable** (RECOMMENDED) | `stable` | _empty_ | _empty_ | `debian:trixie` | Always use newest stable syslog-ng and base OS |
| **Publish latest nightly** | `nightly` | _empty_ | _empty_ | `debian:trixie` | Development builds with cutting-edge code |
| Rebuild with OS security patches only | `stable` | _empty_ | `4.5.0-1` | `debian:trixie` | Lock syslog-ng version, update base OS |
| Test before publishing | any | any | any | any | Set `pkg-push: false` |

**Key Parameters:**
- `pkg-type`: Package repository type - `stable` or `nightly`
- `pkg-version`: Controls Docker image **tags** (e.g., `4.5.0` → tag `balabit/syslog-ng:4.5.0`). Leave empty for latest.
- `apt-pkg-version`: Controls **which APT package** gets installed (Debian format with revision, e.g., `4.5.0-1`). Leave empty for latest from repo.
- `base-image`: Always pulls the latest of this tag (e.g., latest `debian:trixie`)

**Version Format Difference:**
```yaml
pkg-version: '4.5.0'         # Docker tag format (no revision)
apt-pkg-version: '4.5.0-1'   # Debian package format (includes -revision)
```

**Examples:**

```yaml
# Latest stable (most common)
with:
  pkg-type: stable
  base-image: 'debian:trixie'

# Latest nightly
with:
  pkg-type: nightly
  base-image: 'debian:trixie'

# Locked version with latest base OS
with:
  pkg-type: stable
  pkg-version: '4.5.0'
  apt-pkg-version: '4.5.0-1'
  base-image: 'debian:trixie'
```

## Overview

The workflow now supports **reproducible and flexible builds** that allow you to:
- **Publish with latest syslog-ng version** and latest base OS (most common use case)
- Re-publish stable Docker images with the same syslog-ng package version
- Use updated base OS images to get latest security patches
- Lock to specific package versions for true reproducibility
- Manually trigger rebuilds via GitHub Actions UI

## Use Cases

### 1. Publish Latest Versions (RECOMMENDED)

```yaml
with:
  pkg-type: stable
  base-image: 'debian:trixie'
  # All version parameters empty = latest stable syslog-ng + latest base OS
```

### 2. Rebuild Specific Version with Latest Base OS (Security Updates)

**Use this when you need to keep a specific syslog-ng version but want the latest OS security patches.**

```yaml
with:
  pkg-type: stable
  pkg-version: '4.5.0'
  apt-pkg-version: '4.5.0-1'  # Lock to specific APT package version
  base-image: 'debian:trixie'
  pkg-push: true
```

### 3. Publish Nightly Builds

```yaml
with:
  pkg-type: nightly
  base-image: 'debian:trixie'
```

### 4. Switch Base OS Distribution

```yaml
# Ubuntu instead of Debian
with:
  pkg-type: stable
  pkg-version: '4.5.0'
  apt-pkg-version: '4.5.0-1'
  base-image: 'ubuntu:24.04'
```

### 5. Test Build Without Publishing

```yaml
with:
  pkg-type: stable
  pkg-version: '4.5.0'
  base-image: 'debian:trixie'
  pkg-push: false  # Test without pushing
```

### 6. Scheduled Weekly Rebuilds (Production)

**Automated weekly builds with latest versions:**

```yaml
name: Weekly Docker Publish

on:
  schedule:
    - cron: '0 12 * * 0'  # Every Sunday at 12:00 UTC
  workflow_dispatch:

jobs:
  publish:
    uses: ./.github/workflows/publish-docker-image.yml
    with:
      pkg-type: stable
      base-image: 'debian:trixie'
      pkg-push: true
    secrets: inherit
```

Included as `.github/workflows/weekly-docker-publish.yml`

### 7. Scheduled Monthly Security Rebuilds

```yaml
name: Monthly Security Rebuild

on:
  schedule:
    - cron: '0 3 1 * *'  # 1st of each month
  workflow_dispatch:

jobs:
  rebuild:
    strategy:
      matrix:
        version: ['4.5.0', '4.6.0']
        version-lock: ['4.5.0-1', '4.6.0-1']
    uses: ./.github/workflows/publish-docker-image.yml
    with:
      pkg-type: stable
      pkg-version: ${{ matrix.version }}
      apt-pkg-version: ${{ matrix.version-lock }}
      base-image: 'debian:trixie'
      pkg-push: true
    secrets: inherit
```

## Parameters Reference

### Inputs

| Parameter | Required | Type | Default | Description |
|-----------|----------|------|---------|-------------|
| `pkg-type` | Yes | string | - | Package type: `stable` or `nightly` |
| `pkg-version` | No | string | _empty_ | Docker image tag version (e.g., `4.5.0`). **Leave empty to use latest version** (recommended for regular builds) |
| `apt-pkg-version` | No | string | _empty_ | Exact Debian package version to install (e.g., `4.5.0-1` with revision). **Leave empty to install latest from repository** (recommended) |
| `base-image` | No | string | `debian:trixie` | Base OS Docker image. Always pulls latest tag |
| `pkg-push` | No | boolean | `true` | Push to Docker Hub. Set to `false` for testing |

**💡 TIP:** For most use cases, leave both `pkg-version` and `apt-pkg-version` empty to automatically use the latest stable syslog-ng version.

### Secrets

| Secret | Required | Description |
|--------|----------|-------------|
| `dockerhub-username` | Yes | Docker Hub username |
| `dockerhub-password` | Yes | Docker Hub password/token |

## How It Works

### 1. Latest Version Behavior (Default)

When `apt-pkg-version` is **not specified** (most common):

```dockerfile
# Installs latest syslog-ng from the repository
apt-get install -y syslog-ng
```

This means:
- ✅ Always gets the **newest syslog-ng** from the stable repository
- ✅ Includes all latest features and bug fixes
- ✅ Automatically updated when new versions are published
- ✅ Perfect for CI/CD and regular releases

### 2. Base Image Updates

When you specify `base-image: 'debian:trixie'`, Docker pulls the **latest** `debian:trixie` image each time. This means:
- Base OS security patches are automatically included
- System libraries are updated
- CVEs in base image are fixed

### 3. Package Version Locking (Optional)

The `apt-pkg-version` parameter pins the exact syslog-ng package version:

```dockerfile
# Without lock (installs latest from repository) - RECOMMENDED
apt-get install -y syslog-ng

# With lock (installs specific version) - For reproducibility
apt-get install -y syslog-ng=4.5.0-1
```

**Use locking when:**
- You need to rebuild with OS security updates only
- You want guaranteed reproducibility
- You're maintaining older versions

**Don't use locking when:**
- You want the latest features and fixes (most cases)
- You're doing regular CI/CD builds
- You want automatic updates

### 4. Reproducibility

To achieve true reproducibility:
1. **Lock syslog-ng version**: Use `apt-pkg-version`
2. **Document base image digest**: After build, note the base image digest for exact reproducibility
3. **Version control**: Commit workflow files with specific parameters

Example of capturing base image digest:
```bash
docker inspect debian:trixie | jq -r '.[0].RepoDigests[0]'
# debian@sha256:abc123...
```

## Finding Package Versions

### APT Package Version Format

The `apt-pkg-version` parameter uses **Debian package version format**:

```
<upstream-version>-<debian-revision>
```

**Examples:**
- `4.5.0-1` = syslog-ng version 4.5.0, Debian package revision 1
- `4.5.0-2` = syslog-ng version 4.5.0, Debian package revision 2 (rebuild/patch)
- `4.6.0-1ubuntu1` = syslog-ng version 4.6.0, Ubuntu-specific package build

**Key Difference:**
- `pkg-version: '4.5.0'` → Controls Docker image tag: `balabit/syslog-ng:4.5.0`
- `apt-pkg-version: '4.5.0-1'` → APT installs: `apt-get install syslog-ng=4.5.0-1`

### How to Find Available Versions

To find actual package versions available in the repository:

```bash
# List available syslog-ng versions in repository
curl -s https://ose-repo.syslog-ng.com/apt/dists/stable/debian-trixie/binary-amd64/Packages.gz \
  | gunzip | grep -A5 "^Package: syslog-ng$" | grep "Version:"

# Example output:
# Version: 4.10.0-1
# Version: 4.10.1-1
# Version: 4.11.0-1

# Or simpler - just download and view the plain text version:
curl -s https://ose-repo.syslog-ng.com/apt/dists/stable/debian-trixie/binary-amd64/Packages \
  | grep -A5 "^Package: syslog-ng$" | grep "Version:"
```

**Or check from within a container:**

```bash
# Show available versions
apt-cache madison syslog-ng

# Show currently installed version
dpkg -l | grep syslog-ng
```

## Best Practices

- **Weekly Security Rebuilds**: Use scheduled workflows to keep base OS updated
- **Testing**: Always test with `pkg-push: false` before production
- **Version Documentation**: Document locked versions in workflow comments
- **CVE Monitoring**: Check GitHub Security tab for Trivy scan results
- **Multi-Architecture**: Builds automatically create AMD64 and ARM64 images

## Troubleshooting

**Package Version Not Found**: Check available versions:
`apt-cache madison syslog-ng` or verify distribution matches repository

**Base Image Pull Failures**: Verify image name and architecture support

**Build Time Increase**: Expected when base images update with security patches

## Migration from Old Workflow

**For regular releases** (leave version fields empty):
```yaml
with:
  pkg-type: stable
  base-image: 'debian:trixie'
```

**For version-locked rebuilds**:
```yaml
with:
  pkg-type: stable
  pkg-version: '4.5.0'
  apt-pkg-version: '4.5.0-1'
  base-image: 'debian:trixie'
```

---

## Quick Decision Guide

| Your Goal | Use This Configuration |
|-----------|----------------------|
| 🚀 **Regular releases with latest versions** | `pkg-type: stable`, leave version fields empty |
| 🔒 **Locked version with OS security updates** | Set `pkg-version` and `apt-pkg-version` |
| 🧪 **Test nightly builds** | `pkg-type: nightly`, leave version fields empty |
| 🐧 **Different OS** | Change `base-image` to `ubuntu:24.04`, etc. |
| 🧪 **Test without publishing** | Set `pkg-push: false` |
