# Crash reports

WxLens prepares a report locally after a crash. On the next successful launch it
offers a preview, **Save report**, and **Send crash report**. Users can reopen the
panel from **Help → Crash reports**. Closing the panel remembers that choice;
nothing is uploaded in the background. Failed sends can be retried explicitly,
and the local report remains available when offline or over the service quota.

## Receiving reports for free

The maintainer's public Sentry DSN is the default `WXLENS_SENTRY_DSN` CMake cache
value in `app/CMakeLists.txt`. It is an ingestion address, not an API token, and
is intentionally included in released binaries. No new SDK/library is required;
Qt Network sends a Sentry event envelope over HTTPS. Only public hosted Sentry
DSNs are accepted, and redirects are not followed.

Use Sentry's **Developer ($0)** plan. As checked on 2026-09-09, it includes one
user, 5,000 errors per month, email notifications and 30-day data retention:
[Sentry pricing](https://sentry.io/pricing/). Keep paid features and paid overages
disabled. When the free quota is exhausted, users can still save a local report.

In the Sentry WxLens project, configure an issue alert for a new issue, with an
email action to the maintainer, and verify the account's email/notification
preferences. An accepted HTTP request confirms ingestion, not email receipt.
The setup test appears as **WxLens crash reporting setup test (not a real crash)**.

To disable delivery in a custom build, configure with `-DWXLENS_SENTRY_DSN=`.
To send a fork's reports elsewhere, pass its public hosted Sentry DSN instead.
Existing build directories preserve their cached value; clear or change that
cache entry when adopting a different destination. Never put a private Sentry API
token or an email password in the application.

## If the app keeps crashing on startup

Open the report-only window. It uses Qt Quick's software renderer and does not
initialize the map, radar providers, or AWS:

```sh
# macOS: run in Terminal after copying the app to Applications
/Applications/WxLens.app/Contents/MacOS/WxLens --crash-report
```

```powershell
# Windows: run from the application's installation folder
.\wxlens-app.exe --crash-report
```

This mode still needs the executable and Qt/QML libraries to load. A loader or
signature failure that prevents any application code from running must be
reported using macOS's native crash report directly.

## Collection and privacy

- macOS: selects the latest valid WxLens `.ips` from
  `~/Library/Logs/DiagnosticReports`, up to 14 days old, verifies its process and
  bundle identity, and retries once after five seconds if no report is available.
  Apple writes these reports independently of WxLens. Source:
  [Apple's JSON crash report format](https://developer.apple.com/documentation/xcode/interpreting-the-json-format-of-a-crash-report).
- Windows: selects the latest complete exception report from
  `logs/wxlens-crash.log` under Qt's AppDataLocation. Shutdown-watchdog dumps are
  excluded. New captures include the crashed app's version.
- Linux: crash capture is not implemented in this slice; the panel does not
  invent a stack trace from an unclean exit or a log message.

The shared report contains selected exception/termination codes, module names,
image UUIDs/offsets and stack symbols, with crash version/OS metadata where
available. The version/OS of the application *sending* the report is labeled
separately as `reporter_*`; old reports are not attributed to the new build.
The UI displays the event payload used for both saving and sending.

Raw operating-system reports, source paths, user/device identifiers, saved
places, credentials, settings, arbitrary application logs and memory dumps are
not attached. Field selection happens locally, before preview or transmission.
Sentry necessarily receives the connection's IP address; the app does not
add a user profile or persistent device identifier. Review before sharing.
This is a selected diagnostic report, not a native minidump or an automatic
symbolication integration; unresolved addresses may require matching debug symbols.

Reports remain in their original OS/app locations. `crash_reporting.toml` stores
only hashes of the most recently reviewed and sent reports, so reopening the app
does not repeatedly prompt for the same crash. No automatic retries or always-send
preference are enabled.

## Verification

`CrashReport.*` tests cover parsing, rejection of unrelated/malformed reports,
field filtering, DSN validation and envelope byte lengths. Windows additionally
checks preview/save equality and persisted dismissal using temporary files.
The macOS packaging workflow separately tests startup from the finished DMG and
preserves diagnostics. A real macOS crash/relaunch still needs native validation.
