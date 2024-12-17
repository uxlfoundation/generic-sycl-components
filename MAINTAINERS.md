# Introduction

This document defines roles in oneMath project.

# Roles and responsibilities

Generic SYCL components project defines three main roles:
 * [Contributor](#contributor)
 * [Domain maintainer](#domain-maintainer)
 * [Architecture maintainer](#architecture-maintainer)

These roles are merit based. Refer to the corresponding section for specific
requirements and the nomination process.

## Contributor

A Contributor invests time and resources to improve the project.
Anyone can become a Contributor by bringing value in one of the following ways:
  * Answer questions from community members.
  * Submit feedback to design proposals.
  * Review and/or test pull requests.
  * Test releases and report bugs.
  * Contribute code, including bug fixes, features implementations,
and performance optimizations.
  * Contribute design proposals.

Responsibilities:
  * Follow the [Code of Conduct](CODE_OF_CONDUCT.md).
  * Follow the project [contributing guidelines](CONTRIBUTING.md).

Privileges:
  * Eligible to join one of the maintainer groups.

## Domain Maintainer

Domain maintainer has responsibility for a specific domain in the project.
Domain maintainers are collectively responsible for developing and maintaining their domain,
including reviewing all changes to their domain and indicating
whether those changes are ready to merge. They have a track record of
contribution and review in the project.

Responsibilities:
  * Follow the [Code of Conduct](CODE_OF_CONDUCT.md).
  * Follow and enforce the project [contributing guidelines](CONTRIBUTING.md).
  * Review pull requests in their specific domain.
  * Support and guide Contributors.

Requirements:
  * Experience as Contributor in the specific domain for at least 6 months.
  * Track record of accepted code contributions to a specific domain.
  * Track record of contributions to the code review process.
  * Demonstrated in-depth knowledge of the specific domain.
  * Commits to being responsible for that specific domain.

Privileges:
  * PR approval counts towards approval requirements for a specific domain.
  * Can promote fully approved Pull Requests to the `main` branch.
  * Can recommend Contributors to become Domain maintainer.

The process of becoming a Domain maintainer is:
1. A Contributor requests to join corresponding Domain maintainer GitHub team.
2. At least one specific Domain maintainers approve the request.

### List of GitHub teams for Domain maintainers

| GitHub team name | Domain maintainers |
:-----------|:------------|
| @uxlfoundation/onemath-blas-write | oneMath BLAS maintainers |
| @uxlfoundation/generic-sycl-components-write | Reviewing SYCL kernels maintainers |

## Architecture Maintainer
Architecture maintainers are the most established contributors who are responsible for the
project technical direction and participate in making decisions about the
strategy and priorities of the project.

Responsibilities:
  * Follow the [Code of Conduct](CODE_OF_CONDUCT.md).
  * Follow and enforce the project [contributing guidelines](CONTRIBUTING.md)
  * Co-own with other Domain maintainers on the technical direction of a specific domain.
  * Co-own with other Architecture maintainers on the project as a whole, including
determining strategy and policy for the project.
  * Support and guide Contributors and Domain maintainers.

Requirements:
  * Experience as a Domain maintainer or Contributor with focus on the project architecture
for at least 12 months.
  * Commit at least 25% of working time to the project.
  * Track record of major project contributions.
  * Demonstrated deep knowledge of the project architecture and build.
  * Demonstrated broad knowledge of the project across multiple domains.
  * Is able to exercise judgment for the good of the project, independent of
    their employer, friends, or team.

Privileges:
  * Can represent the project in public as a Maintainer.
  * Can recommend Contributor or Domain maintainer to become Architecture maintainers.

Process of becoming a maintainer:
1. A Contributor or Domain maintainer requests to join oneMath Architecture maintainers GitHub team
(@uxlfoundation/onemath-arch-write).
2. At least one of Architecture maintainers approves the request.

