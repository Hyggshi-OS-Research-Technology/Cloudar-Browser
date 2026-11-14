# AdBlock — chặn quảng cáo trên YouTube và các trang web khác

Gồm 2 lớp hoạt động độc lập, bổ sung cho nhau:

## 1. Lớp mạng (network-level) — `features/adblock.py`
`AdBlockInterceptor` (kế thừa `QWebEngineUrlRequestInterceptor`) chặn các
request tới danh sách domain quảng cáo/tracker đã biết, **trước khi request
rời khỏi tiến trình** — áp dụng cho **mọi trang web**, không riêng YouTube.

- `GENERAL_AD_DOMAINS`: doubleclick, googlesyndication, google-analytics,
  outbrain, taboola, criteo, pubmatic, hotjar, mixpanel, v.v. — các mạng
  quảng cáo/tracker phổ biến nhất trên web.
- `YOUTUBE_AD_DOMAINS`: các endpoint quảng cáo/telemetry riêng của YouTube
  (`googleads.g.doubleclick.net`, `youtube.com/pagead`,
  `youtube.com/ptracking`, `youtube.com/api/stats/ads`, `2mdn.net`, ...).
- Có `blocked_count_changed(int)` để hiện số lượng đã chặn ở status bar
  (đã có sẵn UI cho việc này trong `core/browser_window.py`, không cần sửa
  gì thêm — file `browser_window.py` gửi trong patch trước đã import và
  wire sẵn `AdBlockInterceptor` rồi, phần này chỉ còn thiếu đúng file
  `features/adblock.py`).
- Hỗ trợ allowlist theo domain: `set_allowlist([...])`,
  `add_to_allowlist(host)`, `remove_from_allowlist(host)` — dùng cho nút
  "Tắt AdBlock cho trang này" nếu bạn muốn thêm vào UI sau.

### Giới hạn cần biết
YouTube ngày càng ghép một số quảng cáo **ngay trong luồng video** (server-side
ad insertion / SSAI) — không có request riêng để chặn ở tầng mạng. Phần này
được xử lý bằng lớp thứ 2 bên dưới (ẩn UI + tự bấm Skip), không thể chặn
100% quảng cáo dạng ghép luồng chỉ bằng domain-blocking.

## 2. Lớp giao diện (cosmetic) — `features/extensions/adblock/background.js`
Content script, tự động chèn vào mọi trang qua `ExtensionManager` (giống cơ
chế `video_downloader`), gồm:

- **Mọi trang**: chèn CSS ẩn các khối quảng cáo phổ biến
  (`ins.adsbygoogle`, `div[id^="google_ads_iframe"]`,
  `div[id^="div-gpt-ad"]`, `[data-ad-slot]`, iframe doubleclick/
  googlesyndication, ...) + `MutationObserver` dự phòng cho quảng cáo
  chèn động sau khi trang đã tải.
- **Riêng YouTube**:
  - Ẩn quảng cáo masthead, display ad trong feed, companion ad, banner
    khuyến mãi (`ytd-display-ad-renderer`, `ytd-companion-slot-renderer`,
    `#masthead-ad`, `.ytp-ad-overlay-container`, ...) — đây là các phần tử
    DOM tách biệt với video, ẩn không ảnh hưởng phát video.
  - **Tự động bấm nút "Skip Ad"** ngay khi nút xuất hiện.
  - **Tắt tiếng khi quảng cáo không thể bỏ qua đang phát** (`.ad-showing`/
    `.ad-interrupting` trên player), tự bật tiếng lại đúng trạng thái cũ
    khi quảng cáo kết thúc. Không tua/seek video để tránh ảnh hưởng nhầm
    tới nội dung thật nếu detect sai.

## Cách áp dụng
- Copy `features/adblock.py` vào project → **restart app** (code Python
  mới, cần app nạp lại module).
- Copy `features/extensions/adblock/background.js` (và bản sao ở
  `extensions/adblock/background.js`) → reload tab là đủ, không cần
  restart, miễn `ExtensionManager` của bạn quét thư mục `extensions/`
  theo tên thư mục con giống cách `video_downloader` đang hoạt động. Nếu
  `ExtensionManager` của bạn dùng cơ chế khác (vd. cần `manifest.json`
  hoặc danh sách đăng ký thủ công), báo mình biết để chỉnh lại cho khớp.
- Toggle bật/tắt lớp mạng: setting `adblock_enabled` đã có sẵn trong
  Settings (đúng key mà `browser_window.py` bản trước đã đọc).
- Toggle bật/tắt lớp cosmetic (content script): qua trang quản lý
  extension hiện có của bạn (`cloudar://extensions`), y hệt cách bật/tắt
  extension `video_downloader`.

## Có thể mở rộng thêm (nếu cần)
- Thêm nút khiên 🛡️ trên toolbar để bật/tắt AdBlock nhanh cho từng site
  (đã chừa sẵn API `add_to_allowlist`/`remove_from_allowlist` ở
  `features/adblock.py` cho việc này).
- Tải danh sách domain từ EasyList/EasyPrivacy định kỳ thay vì danh sách
  tĩnh trong code, nếu muốn độ phủ cao hơn.
