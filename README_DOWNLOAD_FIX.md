# Sửa lỗi "Ask before download" và "Download folder" không có tác dụng

## Nguyên nhân
`features/download_manager.py` là nơi thực sự xử lý mọi lượt tải file
thật của trình duyệt (`profile.downloadRequested.connect(self.download_manager.handle_download)`
trong `core/browser_window.py`). Hàm `handle_download()` trước đây:

- **Không hề đọc** `ask_download` — nên dù bạn bật "Ask before download"
  trong Settings, không có hộp thoại nào hiện ra; file cứ lặng lẽ lưu vào
  thư mục mặc định của Qt.
- **Không hề đọc** `download_location` — đây là key mà trang Settings
  HTML (`resources/pages/settings/index.html` + `settings_backend.py`)
  thực sự ghi vào khi bạn gõ/chọn "Download folder". Code cũ chỉ kiểm tra
  2 key khác là `force_download_directory`/`forced_download_path`, vốn
  thuộc về một dialog Settings kiểu Qt native cũ hơn
  (`features/settings_dialog.py`) — dialog này **đã không còn được khởi
  tạo ở đâu trong `browser_window.py` nữa** (chỉ còn `import` chứ không
  gọi), nên 2 key đó gần như luôn rỗng/mặc định trong thực tế. Kết quả:
  đổi "Download folder" trong Settings không ảnh hưởng gì tới nơi file
  thật sự được lưu.

## Đã sửa
`handle_download()` giờ đọc đúng các key mà Settings HTML ghi ra, theo
thứ tự ưu tiên:

1. `force_download_directory` + `forced_download_path` (nếu còn ai đó đặt
   thủ công trong `settings.json`) — override tuyệt đối, giữ lại để không
   phá vỡ gì nếu bạn từng dùng dialog cũ.
2. `download_location` — thư mục tải mặc định bạn chọn trong Settings.
3. `ask_download` — nếu bật, hiện hộp thoại "Save File" (giống trình
   duyệt thường) để chọn đúng nơi lưu + tên file cho **từng lượt tải**,
   trước khi `download.accept()`. Nếu bạn bấm Cancel ở hộp thoại, lượt tải
   bị huỷ luôn (`download.cancel()`) thay vì âm thầm lưu mặc định.

Sau khi xác định xong thư mục/tên file, code gọi
`download.setDownloadDirectory(...)` và `download.setDownloadFileName(...)`
(API của `QWebEngineDownloadRequest`) trước khi `download.accept()`, nên
Qt thực sự lưu đúng chỗ thay vì chỉ lưu vào path mặc định như trước.

## Cách áp dụng
Copy đè `features/download_manager.py` vào project → **khởi động lại
app** (đây là code Python cấu hình lúc `DownloadManager` xử lý tín hiệu
`downloadRequested`, cần nạp lại module).

## Lưu ý
- Đây là bản vá cho các lượt tải **thông thường** của trình duyệt (click
  link tải file, `<a download>`, v.v.). Lượt tải video YouTube qua
  `features/youtube_downloader.py` là một luồng riêng, đã tự đọc
  `ask_download` từ trước (không bị ảnh hưởng, không cần sửa gì thêm).
- Nếu `download_location` bạn đặt không phải là một thư mục tồn tại
  (gõ sai đường dẫn, thư mục bị xoá, ...), code sẽ tự bỏ qua và dùng
  thư mục mặc định của Qt để tránh lỗi treo/crash.
