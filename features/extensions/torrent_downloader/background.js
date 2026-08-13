(function() {
    'use strict';

    // ─── Torrent Downloader Extension ──────────────────────────────────
    // Detects .torrent file links and magnet: links on any page, shows a
    // floating button, and lets the user download them directly through
    // the browser's built-in download manager (no external software needed).
    //
    // For magnet: links, the extension communicates with a Python bridge
    // (TorrentDownloaderBridge) over QWebChannel, which uses libtorrent
    // (if available) or falls back to opening the magnet in a new tab
    // with a web-based torrent client.

    // ─── Detection ─────────────────────────────────────────────────────
    function getTorrentSources() {
        const sources = [];

        // 1. Direct <a> links to .torrent files
        document.querySelectorAll('a[href*=".torrent"]').forEach(function(a) {
            const href = a.getAttribute('href');
            if (!href) return;
            const url = resolveUrl(href);
            if (sources.some(s => s.url === url)) return;
            sources.push({
                url: url,
                name: extractFileName(url, a.textContent.trim() || 'torrent.torrent'),
                type: 'torrent',
                mime: 'application/x-bittorrent'
            });
        });

        // 2. Magnet links
        document.querySelectorAll('a[href^="magnet:"]').forEach(function(a) {
            const href = a.getAttribute('href');
            if (!href) return;
            if (sources.some(s => s.url === href)) return;
            const name = extractMagnetName(href) || a.textContent.trim() || 'Magnet Link';
            sources.push({
                url: href,
                name: name,
                type: 'magnet',
                mime: ''
            });
        });

        // 3. Also check for torrent/magnet links in data attributes or
        //    dynamically loaded content (e.g. data-href, data-url)
        document.querySelectorAll('[data-torrent], [data-magnet], [data-torrent-url]').forEach(function(el) {
            const href = el.getAttribute('data-torrent') || el.getAttribute('data-magnet') || el.getAttribute('data-torrent-url');
            if (!href) return;
            const url = href.startsWith('magnet:') ? href : resolveUrl(href);
            if (sources.some(s => s.url === url)) return;
            const isMagnet = href.startsWith('magnet:');
            sources.push({
                url: url,
                name: extractFileName(url, el.textContent.trim() || (isMagnet ? 'Magnet Link' : 'torrent.torrent')),
                type: isMagnet ? 'magnet' : 'torrent',
                mime: isMagnet ? '' : 'application/x-bittorrent'
            });
        });

        return sources;
    }

    function extractFileName(url, fallback) {
        try {
            const u = new URL(url, window.location.href);
            const pathParts = u.pathname.split('/');
            let last = pathParts[pathParts.length - 1];
            if (last && last.includes('.')) {
                try { last = decodeURIComponent(last); } catch(e) {}
                const qIndex = last.indexOf('?');
                if (qIndex > -1) last = last.substring(0, qIndex);
                return last;
            }
        } catch(e) {}
        return fallback || 'torrent.torrent';
    }

    function extractMagnetName(magnetUri) {
        try {
            const u = new URL(magnetUri);
            const dn = u.searchParams.get('dn');
            if (dn) return decodeURIComponent(dn.replace(/\+/g, ' ')) + '.torrent';
        } catch(e) {}
        return null;
    }

    function resolveUrl(url) {
        try {
            return new URL(url, window.location.href).href;
        } catch(e) {
            return url;
        }
    }

    // ─── Python bridge (QWebChannel) ──────────────────────────────────
    let torrentBridge = null;
    let torrentBridgeRequestInFlight = false;
    let bridgeAvailable = false;

    function ensureTorrentBridge(callback) {
        if (torrentBridge) { callback(torrentBridge); return; }
        if (torrentBridgeRequestInFlight) {
            setTimeout(function() { ensureTorrentBridge(callback); }, 200);
            return;
        }
        if (typeof qt === 'undefined' || !qt.webChannelTransport || typeof QWebChannel === 'undefined') {
            callback(null);
            return;
        }
        torrentBridgeRequestInFlight = true;
        try {
            new QWebChannel(qt.webChannelTransport, function(channel) {
                torrentBridgeRequestInFlight = false;
                torrentBridge = (channel && channel.objects && channel.objects.torrentDownloader) || null;
                if (torrentBridge) {
                    bridgeAvailable = true;
                    torrentBridge.isAvailable(function(avail) {
                        bridgeAvailable = avail;
                    });
                    torrentBridge.downloadFinished.connect(function(url, success, message) {
                        if (success) {
                            showNotification('✅ Torrent download started: ' + message, 6000);
                        } else if (message === 'Cancelled') {
                            showNotification('⏹️ Cancelled', 3000);
                        } else {
                            showNotification('❌ Failed: ' + (message || 'unknown error'), 6000);
                        }
                    });
                }
                callback(torrentBridge);
            });
        } catch (e) {
            torrentBridgeRequestInFlight = false;
            callback(null);
        }
    }

    function downloadMagnet(magnetUri, suggestedName) {
        ensureTorrentBridge(function(bridge) {
            if (!bridge) {
                // Fallback: open magnet link in a new tab (web torrent client)
                showNotification('⚠️ Opening magnet link in new tab...');
                window.open(magnetUri, '_blank');
                return;
            }
            bridge.isAvailable(function(avail) {
                if (!avail) {
                    showNotification('⚠️ libtorrent not installed. Opening magnet in new tab...');
                    window.open(magnetUri, '_blank');
                    return;
                }
                bridge.downloadMagnet(magnetUri, suggestedName || '');
            });
        });
    }

    // ─── Modal UI ─────────────────────────────────────────────────────
    let modalActive = false;

    function createTorrentModal(sources) {
        if (modalActive) return;
        modalActive = true;

        const overlay = document.createElement('div');
        overlay.id = 'torrent-downloader-overlay';
        overlay.style.cssText = `
            position: fixed; top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0,0,0,0.6); z-index: 999999;
            display: flex; align-items: center; justify-content: center;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        `;

        const modal = document.createElement('div');
        modal.style.cssText = `
            background: #1e1e2e; border-radius: 12px; padding: 24px;
            max-width: 520px; width: 90%; max-height: 80vh; overflow-y: auto;
            box-shadow: 0 8px 32px rgba(0,0,0,0.4); color: #cdd6f4;
        `;

        // Close button
        const closeBtn = document.createElement('button');
        closeBtn.textContent = '\u00D7';
        closeBtn.style.cssText = `
            float: right; background: none; border: none; color: #888;
            font-size: 24px; cursor: pointer; padding: 0; line-height: 1;
        `;
        closeBtn.onclick = closeModal;
        modal.appendChild(closeBtn);

        // Title
        const title = document.createElement('h2');
        const hasTorrents = sources.some(s => s.type === 'torrent');
        const hasMagnets = sources.some(s => s.type === 'magnet');
        title.textContent = hasTorrents && hasMagnets ? '🧲 Torrents & Magnets Detected'
            : hasMagnets ? '🧲 Magnet Links Detected' : '⬇ Torrent Files Detected';
        title.style.cssText = 'margin: 0 0 8px 0; font-size: 18px; font-weight: 600; color: #fff;';
        modal.appendChild(title);

        const subtitle = document.createElement('p');
        subtitle.textContent = sources.length > 1
            ? 'This page has ' + sources.length + ' downloadable torrents. Download them?'
            : 'This page has a downloadable torrent. Download it?';
        subtitle.style.cssText = 'margin: 0 0 16px 0; font-size: 14px; color: #a6adc8;';
        modal.appendChild(subtitle);

        // Select-all header
        const selectHeader = document.createElement('div');
        selectHeader.style.cssText = 'display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;';

        const selectAllLbl = document.createElement('label');
        selectAllLbl.style.cssText = 'display: flex; align-items: center; gap: 6px; font-size: 12px; color: #a6adc8; cursor: pointer;';
        const selectAllCb = document.createElement('input');
        selectAllCb.type = 'checkbox';
        selectAllCb.checked = true;
        selectAllCb.style.cssText = 'cursor: pointer;';
        selectAllCb.onchange = function() {
            list.querySelectorAll('.torrent-item-row').forEach(function(row) {
                row.querySelector('.torrent-item-cb').checked = selectAllCb.checked;
            });
            updateSelectedCount();
        };
        selectAllLbl.appendChild(selectAllCb);
        selectAllLbl.appendChild(document.createTextNode('Select All'));
        selectHeader.appendChild(selectAllLbl);

        const countLbl = document.createElement('div');
        countLbl.style.cssText = 'font-size: 12px; color: #a6adc8;';
        selectHeader.appendChild(countLbl);
        modal.appendChild(selectHeader);

        // Torrent list
        const list = document.createElement('div');
        list.style.cssText = 'display: flex; flex-direction: column; gap: 8px;';

        sources.forEach(function(source, index) {
            const item = document.createElement('div');
            item.className = 'torrent-item-row';
            item.dataset.index = String(index);
            item.style.cssText = `
                display: flex; align-items: center; justify-content: space-between;
                padding: 10px 12px; background: #181825; border-radius: 8px;
                border: 1px solid #313244;
            `;

            const itemCb = document.createElement('input');
            itemCb.type = 'checkbox';
            itemCb.className = 'torrent-item-cb';
            itemCb.checked = true;
            itemCb.style.cssText = 'margin-right: 10px; flex-shrink: 0; cursor: pointer;';
            itemCb.onchange = updateSelectedCount;
            item.appendChild(itemCb);

            // Icon
            const icon = document.createElement('span');
            icon.textContent = source.type === 'magnet' ? '🧲' : '⬇';
            icon.style.cssText = 'margin-right: 10px; font-size: 16px; flex-shrink: 0;';
            item.appendChild(icon);

            // Info
            const info = document.createElement('div');
            info.style.cssText = 'flex: 1; min-width: 0; margin-right: 8px;';

            const name = document.createElement('div');
            name.textContent = source.name;
            name.style.cssText = `
                font-size: 13px; font-weight: 500; color: #fff;
                overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
            `;
            info.appendChild(name);

            const details = document.createElement('div');
            details.style.cssText = 'font-size: 11px; color: #a6adc8; margin-top: 2px;';
            if (source.type === 'magnet') {
                details.textContent = '🧲 Magnet Link';
            } else {
                details.textContent = '⬇ Torrent File';
            }
            info.appendChild(details);

            item.appendChild(info);

            // Download button
            const dlBtn = document.createElement('button');
            dlBtn.textContent = '⬇';
            dlBtn.title = 'Download ' + source.name;
            dlBtn.style.cssText = `
                background: #313244; border: none; border-radius: 6px;
                color: #cdd6f4; font-size: 16px; cursor: pointer;
                padding: 6px 10px; flex-shrink: 0; transition: background 0.2s;
            `;
            dlBtn.onmouseover = function() { this.style.background = '#45475a'; };
            dlBtn.onmouseout = function() { this.style.background = '#313244'; };
            dlBtn.onclick = function(e) {
                e.stopPropagation();
                triggerTorrentDownload(source);
                closeModal();
            };
            item.appendChild(dlBtn);

            list.appendChild(item);
        });

        modal.appendChild(list);

        function updateSelectedCount() {
            const rows = Array.from(list.querySelectorAll('.torrent-item-row'));
            const checked = rows.filter(function(r) { return r.querySelector('.torrent-item-cb').checked; });
            countLbl.textContent = checked.length + ' / ' + rows.length + ' selected';
            downloadSelectedBtn.disabled = checked.length === 0;
            downloadSelectedBtn.style.opacity = checked.length === 0 ? '0.5' : '1';
            downloadSelectedBtn.style.cursor = checked.length === 0 ? 'not-allowed' : 'pointer';
            selectAllCb.checked = rows.length > 0 && checked.length === rows.length;
        }

        // Footer buttons
        const footer = document.createElement('div');
        footer.style.cssText = `
            display: flex; justify-content: flex-end; gap: 8px;
            margin-top: 16px; padding-top: 12px; border-top: 1px solid #313244;
        `;

        const skipBtn = document.createElement('button');
        skipBtn.textContent = 'Not Now';
        skipBtn.style.cssText = `
            padding: 8px 16px; background: transparent; border: 1px solid #45475a;
            border-radius: 8px; color: #a6adc8; font-size: 13px; cursor: pointer;
            transition: background 0.2s;
        `;
        skipBtn.onmouseover = function() { this.style.background = '#313244'; };
        skipBtn.onmouseout = function() { this.style.background = 'transparent'; };
        skipBtn.onclick = closeModal;
        footer.appendChild(skipBtn);

        const downloadSelectedBtn = document.createElement('button');
        downloadSelectedBtn.textContent = '⬇ Download Selected';
        downloadSelectedBtn.style.cssText = `
            padding: 8px 16px; background: #5865f2; border: none;
            border-radius: 8px; color: white; font-size: 13px; font-weight: 500;
            cursor: pointer; transition: background 0.2s;
        `;
        downloadSelectedBtn.onmouseover = function() { this.style.background = '#4752c4'; };
        downloadSelectedBtn.onmouseout = function() { this.style.background = '#5865f2'; };
        downloadSelectedBtn.onclick = function() {
            if (downloadSelectedBtn.disabled) return;
            list.querySelectorAll('.torrent-item-row').forEach(function(row) {
                if (!row.querySelector('.torrent-item-cb').checked) return;
                triggerTorrentDownload(sources[parseInt(row.dataset.index, 10)]);
            });
            closeModal();
        };
        footer.appendChild(downloadSelectedBtn);

        modal.appendChild(footer);
        overlay.appendChild(modal);
        document.body.appendChild(overlay);

        updateSelectedCount();

        function closeModal() {
            modalActive = false;
            if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
        }
    }

    // ─── Floating button ──────────────────────────────────────────────
    let fabShown = false;

    function maybeShowTorrentFab() {
        if (fabShown) return;
        const sources = getTorrentSources();
        if (sources.length === 0) return;
        fabShown = true;

        const existing = document.getElementById('torrent-downloader-fab');
        if (existing) existing.remove();

        const fab = document.createElement('button');
        fab.id = 'torrent-downloader-fab';
        fab.title = 'Download torrents on this page';
        fab.textContent = '🧲';
        fab.style.cssText = `
            position: fixed; bottom: 24px; right: 24px; width: 44px; height: 44px;
            border-radius: 50%; background: #5865f2; color: white; border: none;
            font-size: 18px; cursor: pointer; z-index: 999998;
            box-shadow: 0 4px 12px rgba(0,0,0,0.35);
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        `;
        fab.onmouseover = function() { this.style.background = '#4752c4'; };
        fab.onmouseout = function() { this.style.background = '#5865f2'; };
        fab.onclick = function() {
            const fresh = getTorrentSources();
            if (fresh.length === 0) {
                showNotification('No torrents found on this page.');
                return;
            }
            createTorrentModal(fresh);
        };
        document.body.appendChild(fab);
    }

    // ─── Download function ────────────────────────────────────────────
    function triggerTorrentDownload(source) {
        const url = resolveUrl(source.url);

        if (source.type === 'magnet') {
            downloadMagnet(url, source.name);
            return;
        }

        // For .torrent files: use the browser's native download
        // by creating a temporary <a> element. The browser's
        // download manager will handle it.
        const a = document.createElement('a');
        a.href = url;
        a.download = source.name;
        a.style.display = 'none';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);

        showNotification('✅ Downloading: ' + source.name);
    }

    // ─── Toast notification ───────────────────────────────────────────
    function showNotification(message, durationMs) {
        durationMs = durationMs || 3000;
        const existing = document.getElementById('torrent-downloader-toast');
        if (existing) existing.remove();

        const toast = document.createElement('div');
        toast.id = 'torrent-downloader-toast';
        toast.textContent = message;
        toast.style.cssText = `
            position: fixed; bottom: 24px; right: 24px; max-width: 420px;
            word-break: break-all; background: #1e1e2e; color: #cdd6f4;
            padding: 12px 20px; border-radius: 8px;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            font-size: 14px; z-index: 1000000;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            border: 1px solid #313244; animation: torrentSlideIn 0.3s ease;
        `;

        const style = document.getElementById('torrent-downloader-style');
        if (!style) {
            const s = document.createElement('style');
            s.id = 'torrent-downloader-style';
            s.textContent = `
                @keyframes torrentSlideIn {
                    from { transform: translateX(100px); opacity: 0; }
                    to { transform: translateX(0); opacity: 1; }
                }
                @keyframes torrentFadeOut {
                    from { opacity: 1; }
                    to { opacity: 0; }
                }
            `;
            document.head.appendChild(s);
        }

        document.body.appendChild(toast);
        setTimeout(function() {
            toast.style.animation = 'torrentFadeOut 0.3s ease';
            setTimeout(function() {
                if (toast.parentNode) toast.parentNode.removeChild(toast);
            }, 300);
        }, durationMs);
    }

    // ─── Main detection logic ─────────────────────────────────────────
    let initialCheckDone = false;

    function checkForTorrents() {
        // Don't show on internal browser pages
        if (window.location.protocol === 'cloudar:' || 
            window.location.protocol === 'about:' ||
            window.location.hostname === 'localhost' ||
            window.location.hostname === '127.0.0.1') {
            return;
        }

        const sources = getTorrentSources();
        if (sources.length > 0) {
            setTimeout(function() {
                createTorrentModal(sources);
            }, 500);
        }

        // Always show the floating button if torrents exist
        setTimeout(maybeShowTorrentFab, 700);
    }

    function onPageReady() {
        if (initialCheckDone) return;
        initialCheckDone = true;
        checkForTorrents();
    }

    // Check when document is ready
    if (document.readyState === 'complete') {
        onPageReady();
    } else {
        window.addEventListener('load', onPageReady);
        document.addEventListener('DOMContentLoaded', function() {
            setTimeout(onPageReady, 1500);
        });
    }

    // Watch for dynamically added torrent links
    const observer = new MutationObserver(function(mutations) {
        if (initialCheckDone) return;
        for (let m of mutations) {
            for (let node of m.addedNodes) {
                if (node.nodeType === 1) {
                    if (node.tagName === 'A' && (
                        (node.href && node.href.includes('.torrent')) ||
                        (node.href && node.href.startsWith('magnet:'))
                    )) {
                        setTimeout(checkForTorrents, 2000);
                        return;
                    }
                    if (node.querySelectorAll) {
                        const hasTorrent = node.querySelectorAll('a[href*=".torrent"], a[href^="magnet:"]').length > 0;
                        if (hasTorrent) {
                            setTimeout(checkForTorrents, 2000);
                            return;
                        }
                    }
                }
            }
        }
    });

    observer.observe(document.body || document.documentElement, {
        childList: true,
        subtree: true
    });

})();