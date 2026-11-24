#pragma once
// CONTRACT owner=domain layer=domain
// invariant: 已发布的列 ID 只追加，不改序也不复用；旧七列文档始终可读
// forbidden: 不得重排、删除或复用已有 ID；不得改变 Count 的语义
// lifecycle: 仅作编译期常量使用，不参与运行时状态
// tests: domain_contract_tests, connection_view_tests
// spec: docs/development/feature-contracts.md
namespace mlash::ConnectionColumns {
// Stable persisted IDs. Append only; the original seven-column document remains
// readable and new optional columns are hidden until explicitly enabled.
enum Id { Target, Network, Rule, Chain, Bytes, Rates, State, Process, Source,
          DestinationIp, Started, Upload, Download, UploadRate, DownloadRate, Count };
inline constexpr int LegacyCount=7;
}
