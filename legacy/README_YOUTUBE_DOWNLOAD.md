# Tải video YouTube thật — hướng dẫn áp dụng patch (v8 — bộ lọc, kích thước, chọn ảnh gốc, chọn nhiều)

## Thay đổi ở bản v8
Nâng cấp popup "tải ảnh/video trên trang khác" (v7) theo 4 ý tưởng:

**1. Bộ lọc theo định dạng** — chip checkbox `JPG / PNG / GIF / WebP / SVG /
Khác` xuất hiện phía trên danh sách (chỉ hiện định dạng thực sự có trên
trang). Bỏ chọn một định dạng sẽ ẩn ngay các ảnh thuộc định dạng đó khỏi
danh sách (và khỏi lượt chọn "Download Selected"). Lựa chọn được nhớ lại
giữa các lần mở popup (`localStorage`).

**2. Hiển thị kích thước & dung lượng** — mỗi ảnh hiện `1920×1080 • PNG •
1.8 MB` ngay dưới tên file:
- Kích thước điểm ảnh lấy từ `naturalWidth/naturalHeight` của ảnh đã tải;
  nếu trang có `srcset` với bản phân giải cao hơn bản đang hiển thị, kích
  thước đó được **ước tính** (có dấu `~`) theo tỉ lệ khung hình, vì trình
  duyệt chưa thực sự tải/giải mã bản đó để biết kích thước chính xác.
- Dung lượng **không có sẵn** từ thẻ `<img>` — script tự gửi `HEAD`
  request (hoặc GET với `Range: bytes=0-0` nếu server từ chối HEAD) để đọc
  `Content-Length`/`Content-Range`, cập nhật dần vào từng dòng sau khi
  popup đã hiện (không chặn UI chờ tất cả ảnh tính xong). Nếu bị CORS chặn
  → hiện "không rõ dung lượng".

**3. Chỉ tải ảnh gốc (tránh nhầm thumbnail)** — khi phát hiện `srcset`,
script tự chọn ứng viên **độ phân giải cao nhất** làm URL tải về (thay vì
`src` mặc định trình duyệt chọn theo viewport/DPR — thường là bản nhỏ hơn
ảnh gốc). Ảnh có kích thước ước tính dưới 400px cả hai chiều được đánh dấu
là "thumbnail" và **ẩn mặc định**; checkbox "Bao gồm ảnh nhỏ / thumbnail"
để hiện lại nếu cần. Đây là suy đoán theo kích thước (không phải phân tích
DOM riêng cho từng site như Google Images) — với các trang phức tạp hơn có
thể vẫn cần bật "Bao gồm thumbnail" để thấy hết.

**4. Chọn nhiều ảnh/video để tải cùng lúc** — mỗi dòng có checkbox riêng
(mặc định tick sẵn), có "Chọn tất cả" + đếm số đã chọn, nút chính đổi từ
"Download All" thành **"Download Selected"** (disable khi chưa chọn gì).
Nút tải nhanh ⬇ trên từng dòng vẫn còn để tải ngay 1 ảnh mà không cần tick.

### File sửa thêm ở v8
- `features/extensions/video_downloader/background.js` (+ bản sao ở
  `extensions/`): `parseSrcsetBest()`, `extForUrl()`, `fetchFileSizeLabel()`,
  `getImageSources()` viết lại (chọn URL gốc qua srcset, gắn `ext`/
  `isThumbnail`/kích thước ước tính), `createModal()` viết lại phần danh
  sách + footer (bộ lọc, checkbox chọn nhiều, meta ảnh).

## Thay đổi ở bản v7 (tải ảnh + media trên các trang web khác)
Mở rộng phần "trang web khác" (không phải YouTube) để tải được cả **ảnh**,
không chỉ video:

- `getImageSources()`: quét `img[src]` trên trang, lọc bỏ icon/avatar/pixel
  theo dõi (dưới 96px cả hai chiều), khử trùng lặp theo URL tuyệt đối, ưu
  tiên ảnh lớn nhất nếu trang có quá nhiều ảnh (giới hạn 40 ảnh/lần quét).
- **Không tự động bật popup cho ảnh** — nếu tự động hiện "Đã phát hiện ảnh"
  trên mọi trang thì sẽ làm phiền vì hầu như trang nào cũng có ảnh. Thay
  vào đó:
  - Popup tự động (như trước) **chỉ bật khi trang có video thật**, nhưng
    giờ danh sách trong popup đó gồm cả ảnh tìm thấy trên trang (tiện thể).
  - Thêm **nút tròn nổi 🖼️** góc dưới phải (chỉ hiện khi trang có ít nhất
    1 ảnh/video đáng tải) — bấm vào để mở danh sách đầy đủ ảnh + video trên
    trang, quét lại ngay tại thời điểm bấm để bắt cả nội dung tải động sau
    khi trang load xong.
- Modal giờ hiện **thumbnail nhỏ** cho từng ảnh (dùng lại chính ảnh đã tải
  trên trang, không tốn request thêm), nhãn "🖼️ Ảnh" phân biệt với "🎬 Video
  File".
- Tải ảnh dùng `fetch()` → `blob:` URL thay vì click `<a download>` trực
  tiếp, vì ảnh thường nằm trên CDN khác domain — trình duyệt sẽ **bỏ qua**
  thuộc tính `download` với link cross-origin (chỉ mở ảnh, không lưu file).
  Nếu CDN chặn CORS khiến `fetch()` thất bại, tự động mở ảnh ở tab mới kèm
  hướng dẫn "nhấn phải chuột > Save Image As" để người dùng vẫn lưu được.

### File sửa thêm ở v7
- `features/extensions/video_downloader/background.js` (+ bản sao ở
  `extensions/`): `getImageSources()`, `getMediaSources()`,
  `maybeShowMediaFab()`, `triggerImageDownload()`, cập nhật `createModal`
  để hiện thumbnail + nhãn loại ảnh.

## Thay đổi ở bản v6 (chọn định dạng file MP4/WebM/MKV)
Nhiều video YouTube (nhất là chất lượng cao hoặc video mới) chỉ có sẵn dạng
VP9/Opus (`.webm`), không phải H.264/AAC. Bản trước ép cứng mọi video vào
`.mp4`, nên với các luồng kiểu này yt-dlp/ffmpeg phải ép container không
tương thích → dễ lỗi hoặc phải tái mã hoá chậm.

Giờ popup có thêm dòng chọn **Định dạng**: `MP4` / `WebM` / `MKV` / *Giữ
định dạng gốc*. Lựa chọn được nhớ lại (localStorage) cho lần sau. Quan
trọng: nếu định dạng bạn chọn không tương thích với luồng gốc, trình tải
**tự động lùi về định dạng gốc** (không còn bị lỗi treo giữa chừng) —
Python thử tải với `merge_output_format` được chọn, nếu ffmpeg báo lỗi thì
tự retry một lần không ép container.

- Audio only luôn giữ container gốc (m4a/webm/opus) — không có lựa chọn
  định dạng vì không cần merge.
- Nếu bật "Ask before download", hộp thoại lưu file giờ liệt kê thêm bộ lọc
  `*.mp4;;*.webm;;*.mkv`, và nếu bạn gõ đuôi file khác trong hộp thoại
  (vd. đổi tên thành `.webm`), trình tải sẽ dùng đúng đuôi đó làm định
  dạng ép, ghi đè lựa chọn ở popup.

### File sửa thêm ở v6
- `features/youtube_downloader.py`
  - `downloadVideo(url, suggested_title, quality, container)`: thêm tham
    số `container` ("mp4"/"webm"/"mkv"/"" = giữ gốc).
  - `_download_with_python_module` / `_download_with_cli`: thử tải với
    container được chọn, tự động retry không ép container nếu thất bại.
- `features/extensions/video_downloader/background.js` (+ bản sao ở
  `extensions/`): thêm dropdown chọn định dạng trong `createYouTubeModal`,
  lưu lựa chọn vào `localStorage['cloudarVD_container']`.

## Thay đổi ở bản v5 (chọn chất lượng + thumbnail)
Popup được viết lại hoàn toàn cho video YouTube (`createYouTubeModal` trong
`background.js`):

- **Nhiều chất lượng**: 1080p / 720p / 360p / Audio only, mỗi nút gọi
  `yt-dlp -f "bestvideo[height<=H]+bestaudio/best[...]"` (hoặc `bestaudio/best`
  cho Audio only). Chất lượng nào video gốc không có sẽ hiện mờ + "Không có".
- **Dung lượng ước tính**: Python lấy metadata qua yt-dlp
  (`extract_info(download=False)`), cộng dung lượng format video-only tốt
  nhất ở mỗi mức chất lượng với dung lượng audio-only tốt nhất, hiển thị
  ngay trên từng nút (vd. "720p — 45.2 MB").
- **Thumbnail trong popup**: ảnh đại diện video + tiêu đề + kênh + thời
  lượng, lấy từ metadata thật (không phải ảnh đoán URL).
- **"Đừng hỏi lại cho trang này"**: checkbox ở popup, lưu vào
  `localStorage` theo hostname (`youtube.com`/`youtu.be`). Khi đã bật, thay
  vì tự động hiện popup mỗi lần mở video, chỉ hiện một nút tròn nhỏ (⬇) góc
  dưới phải — bấm vào mới mở lại popup chọn chất lượng. Vẫn tải được bình
  thường, chỉ là không bị làm phiền.

Popup hiện chất lượng ngay (chưa có size) để không phải chờ, rồi tự cập
nhật dung lượng/thumbnail khi Python trả kết quả (`videoInfoReady`/
`videoInfoFailed`, qua bridge `youtubeDownloader.requestVideoInfo(url)`).

### File mới/sửa thêm ở v5
- `features/youtube_downloader.py`
  - `requestVideoInfo(url)` (slot mới) + `videoInfoReady`/`videoInfoFailed`
    (signal mới): lấy title/thumbnail/duration/uploader + size từng mức
    chất lượng, chạy nền (thread riêng, không chặn UI).
  - `downloadVideo(url, suggested_title, quality)`: thêm tham số `quality`
    ("1080p"/"720p"/"360p"/"audio"/"" = best), map sang `-f` selector tương
    ứng của yt-dlp qua `_format_selector_for()`.
  - Audio only tải theo container gốc (m4a/webm), không ép merge mp4.
- `features/extensions/video_downloader/background.js` (+ bản sao ở
  `extensions/`): `createYouTubeModal`, `createFloatingYouTubeButton`,
  `requestYouTubeVideoInfo`, `isDontAskSet`/`setDontAsk`.

## Thay đổi ở bản v4 (tôn trọng "Ask before download")
Trước đây yt-dlp downloader BỎ QUA setting "Ask before download" (`ask_download`)
đã có sẵn trong project, lúc nào cũng lưu thẳng vào `download_location`.

Giờ đã fix: nếu bạn bật "Ask before download" trong Settings > Downloads
(giống ảnh bạn gửi), mỗi lần bấm tải video YouTube sẽ hiện **hộp thoại chọn
nơi lưu** (QFileDialog) y hệt tải file bình thường — có thể đổi cả tên file
lẫn thư mục. Nếu tắt setting đó thì tải thẳng vào `download_location` như cũ.
Nếu bạn bấm Cancel ở hộp thoại, sẽ hiện toast "⏹️ Đã hủy tải xuống" thay vì
báo lỗi.

## Bug đã fix ở bản v2
YouTube bật CSP "Trusted Types", chặn `element.innerHTML = "..."`.
Đã đổi `closeBtn.innerHTML` thành `closeBtn.textContent`.

## Toast hiện đường dẫn lưu (từ bản v3)
Toast tải xong hiện luôn đường dẫn file, đứng 8 giây, bấm vào để copy path.

## Cần cài thêm
```
pip install yt-dlp
```
(hoặc cài binary `yt-dlp` trong PATH).

## File mới
- `features/youtube_downloader.py`
  Bridge Python (`YoutubeDownloaderBridge`), expose qua QWebChannel:
  - `isAvailable()` -> bool
  - `downloadVideo(url, title)` -> kiểm tra `ask_download`, mở dialog nếu
    cần, rồi tải bằng yt-dlp trong thread nền
  - signals: `downloadStarted`, `downloadProgress`, `downloadFinished`

## File đã sửa
- `core/browser_window.py`
  - Import `YoutubeDownloaderBridge`
  - Kênh QWebChannel riêng (`self.youtube_web_channel`), tách biệt khỏi
    `self.web_channel` (settingsBridge/internalBridge nhạy cảm)
  - Chỉ gắn cho `youtube.com`/`youtu.be`, gỡ bỏ ở các trang khác

- `features/extensions/video_downloader/background.js` (+ bản sao ở `extensions/`)
  - Nhúng thư viện chính thức `qwebchannel.js` của Qt
  - Detect `youtube.com/watch`, `/shorts/`, `youtu.be`
  - Theo dõi điều hướng SPA (`yt-navigate-finish` + polling URL)
  - Toast tiến trình tải, kết quả, đường dẫn lưu, và trạng thái hủy
  - Fix Trusted Types: `textContent` thay vì `innerHTML`

## Cách áp dụng
- Copy đè `features/youtube_downloader.py` và `core/browser_window.py` ->
  **BẮT BUỘC restart app** (đổi code Python).
- Copy đè `background.js` (2 bản) -> chỉ cần reload tab, không cần restart.

## Lưu ý bảo mật
Bridge `youtubeDownloader` CHỈ gắn cho trang thuộc `youtube.com`/`youtu.be`.
Trang khác không có quyền truy cập QWebChannel nào cả.
