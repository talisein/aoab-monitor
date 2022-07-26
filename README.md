aoab-monitor
============
[![CI](https://github.com/talisein/aoab-monitor/actions/workflows/main.yml/badge.svg)](https://github.com/talisein/aoab-monitor/actions/workflows/main.yml)

This is a simple tool that queries the [J-Novel Club](https://j-novel.club) API
and fetches all the lastUpdated fields for Ascendence of a Bookworm epubs.

A github action that runs periodically with my credentials, and makes the data
publicly available on the [gh-pages
branch](https://aoabmonitor.talinet.net/updates.json). There's also a
[human-readable output](https://aoabmonitor.talinet.net/).

To be precise, `https://labs.j-novel.club/app/v1/me/library` is queried, and the
`{book.volume.slug, book.lastUpdated}` pair is collected for Ascendance of a
Bookworm.

A second github action runs weekly to populate [stats](https://aoabmonitor.talinet.net/stats).

The output is something like
```json
{
  "ascendance-of-a-bookworm-royal-academy-stories-first-year": 1658778570,
  "ascendance-of-a-bookworm-part-4-volume-7": 1655401109,
  "ascendance-of-a-bookworm-part-4-volume-6": 1650409432,
  "ascendance-of-a-bookworm-part-4-volume-5": 1644451348,
  "ascendance-of-a-bookworm-part-4-volume-4": 1639525217,
  "ascendance-of-a-bookworm-fanbook-2": 1636593885,
  "ascendance-of-a-bookworm-part-4-volume-3": 1635134465,
  "ascendance-of-a-bookworm-part-4-volume-2": 1631059672,
  "ascendance-of-a-bookworm-part-4-volume-1": 1631059671,
  "ascendance-of-a-bookworm": 1629246812,
  "ascendance-of-a-bookworm-volume-2": 1629240247,
  "ascendance-of-a-bookworm-part-1-volume-3": 1627007792,
  "ascendance-of-a-bookworm-part-3-volume-4": 1625197221,
  "ascendance-of-a-bookworm-fanbook-1": 1625086094,
  "ascendance-of-a-bookworm-part-3-volume-5": 1619745864,
  "ascendance-of-a-bookworm-part-2-volume-2": 1618876341,
  "ascendance-of-a-bookworm-part-2-volume-1": 1618861840,
  "ascendance-of-a-bookworm-part-2-volume-4": 1617340566,
  "ascendance-of-a-bookworm-part-3-volume-3": 1614750018,
  "ascendance-of-a-bookworm-part-3-volume-2": 1606981567,
  "ascendance-of-a-bookworm-part-3-volume-1": 1606981535,
  "ascendance-of-a-bookworm-part-2-volume-3": 1606981458
}
```
