#pragma once
namespace mlash::ConnectionColumns {
// Stable persisted IDs. Append only; the original seven-column document remains
// readable and new optional columns are hidden until explicitly enabled.
enum Id { Target, Network, Rule, Chain, Bytes, Rates, State, Process, Source,
          DestinationIp, Started, Upload, Download, UploadRate, DownloadRate, Count };
inline constexpr int LegacyCount=7;
}
